/**
 *  Query.cpp
 * 
 *  Implementation file for the Query class
 *  
 *  @author Emiel Bruijntjes <emiel.bruijntjes@copernica.com>
 *  @copyright 2020 - 2023 Copernica BV
 */

/**
 *  Dependencies
 */
#include <string.h>
#include "../include/dnscpp/query.h"
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <stdexcept>
#include "compressor.h"
#include "../include/dnscpp/type.h"
#include "../include/dnscpp/question.h"
#include "../include/dnscpp/response.h"
#include "../include/dnscpp/decompressed.h"
#include "idgenerator.h"

/**
 *  Begin of namespace
 */
namespace DNS {

/**
 *  We need a generator to get random ids
 *  @var IdGenerator
 */
static IdGenerator randomids;

/**
 *  Constructor
 *  @param  op          the type of operation (normally a regular query)
 *  @param  dname       the domain to lookup
 *  @param  type        record type to look up
 *  @param  bits        bits to include in the query
 *  @param  data        optional data (only for type = ns_o_notify)
 *  @throws std::runtime_error
 */
Query::Query(int op, const char *dname, int type, const Bits &bits, const unsigned char *data) : _size(HFIXEDSZ)
{
    // check if parameters fit in the header
    if (type < 0 || type > 65535) throw std::runtime_error("invalid type passed to dns query");

    // make sure buffer is completely filled with zero's
    std::fill(_buffer.begin(), _buffer.end(), 0);
    
    // for simpler access to the header-properties, we use a local variable
    HEADER *header = (HEADER *)_buffer.data();

    // store the opcode
    header->opcode = op;
    
    // bit to ask for a recursive lookup
    header->rd = bits.RD();
    
    // bits to ask for the server to include verify info, or to avoid verification by the server
    header->ad = bits.AD();
    header->cd = bits.CD();
    
    // no error
    header->rcode = ns_r_noerror;

    // use a random ID (because it is random anyway we do not call htons())
    header->id = randomids.generate();
    
    // Perform opcode specific processing
    switch (op) {
    case NS_NOTIFY_OP:
    case QUERY:         break;
    default:            throw std::runtime_error("invalid dns operation");
    }
    
    // we will be compressing data
    Compressor compressor(_buffer.data());

    // compress the name in the buffer
    auto size = compressor.add(dname, end(), remaining());
    
    // check for success
    if (size < 0) throw std::runtime_error("failed domain name compression");
    
    // update the buffer size
    _size += size;
    
    // add the type and dns class
    put16(type);
    put16(ns_c_in);
    
    // only one record is in the query-part
    header->qdcount = htons(1);
    
    // pseudo-loop to be able to break out
    do
    {
        // done if there is no extra part
        if (op == QUERY || data == nullptr) break;
        
        // make an additional record for completion domain.  */
        size = compressor.add((char *)data, end(), remaining());
        
        // check for success
        if (size < 0) throw std::runtime_error("failed data name compression");
        
        // add the further data
        put16(T_NULL);
        put16(ns_c_in);
        put32(0);
        put16(0);
        
        // one argument was added
        header->arcount = htons(1);
    }
    while (false);
    
    // add the edns-pseudo-section
    edns(bits.dnssec());
}

/**
 *  Does this query contain a specific record as question?
 *  @param  record
 *  @return bool
 */
bool Query::contains(const Question &record) const
{
    // start of the buffer
    auto *current = _buffer.data() + HFIXEDSZ;
    
    // check all questions
    for (size_t i = 0; i < questions(); ++i)
    {
        // prevent errors if name could not be extracted
        try
        {
            // decompress the name
            Decompressed name(_buffer.data(), end(), current);
        
            // update current pointer to pass the consumed bytes 
            current += name.consumed();
        
            // check if we still have room for two uint16's
            if (end() - current < 4) break;
            
            // get the next two shorts
            uint16_t type = ns_get16(current);
            uint16_t dnsclass = ns_get16(current + 2);
            
            // update current pointer
            current += 4;
            
            // now run the tests
            if (type != record.type()) continue;
            if (dnsclass != record.dnsclass()) continue;
            
            // type and class are then same, we just need the same name
            if (ns_samename(name, record.name()) > 0) return true;
        }
        catch (...)
        {
            // ignore the unparsable record
            continue;
        }
    }
        
    // record not found
    return false;
}
    
/**
 *  Helper method to add the edns pseudo-section
 *  The original DNS protocol defined a message format that turned out
 *  to be a little bit too small, especially for DNSSEC, which required
 *  some additional properties and flags to be set. The EDNS specification
 *  solves this by allowing an extra pseudo-record to be added to each
 *  message with room for some additional flags and properties. This method
 *  adds that extra pseudo-section to the query.
 *  @param  dnssec      do we want to have DNSSEC responses?
 *  @return bool
 */
bool Query::edns(bool dnssec)
{
    // check if there is enough room
    if (remaining() < 11) return false;

    // the first field for a record is the domain name, this is not in use
    // so we add an empty string (or the root domain, which is the same)
    _buffer[_size++] = 0;
    
    // the type of the pseudo-record is "opt"
    put16(TYPE_OPT);

    // tell the server that we support larger UDP packets
    put16(EDNSPacketSize);
    
    // extended rcode (0 because the normal rcode is good enough) and the 
    // edns version (also 0 because that is the mose up-to-date version)
    _buffer[_size++] = 0;
    _buffer[_size++] = 0;
    
    // extra flags to say that dnssec is supported
    put16(dnssec ? NS_OPT_DNSSEC_OK : 0);
    
    // the next section contains a key-value list of extra options, but
    // since we do not support such extra options, we have a zero-length list
    put16(0);

    // for simpler access to the header-properties, we use a local variable
    HEADER *header = (HEADER *)_buffer.data();

    // increment the counter with number of additional sections
    header->arcount = htons(ntohs(header->arcount)+1);

    // this was a huge success
    return true;
}

/**
 *  The ID inside this object
 *  @return uint16_t
 */
uint16_t Query::id() const noexcept
{
    // use a local variable to access properties
    HEADER *header = (HEADER *)_buffer.data();
    
    // expose the properties
    return ntohs(header->id);
}

/**
 *  The opcode
 *  @return uint8_t
 */
uint8_t Query::opcode() const
{
    // use a local variable to access properties
    HEADER *header = (HEADER *)_buffer.data();
    
    // expose the properties
    return header->opcode;
}

/**
 *  Number of questions
 *  @return size_t
 */
size_t Query::questions() const
{
    // use a local variable to access properties
    HEADER *header = (HEADER *)_buffer.data();
    
    // expose the properties
    return ntohs(header->qdcount);
}

/**
 *  Does a response match with a query, meaning: is the response 
 *  indeed a response to this specific query?
 *  @param  response
 *  @return bool
 */
bool Query::matches(const Response &response) const
{
    // prevent exceptions (parsing a record could fail)
    try
    {
        // the ids must match
        if (response.id() != id()) return false;
        
        // in dynamic update packets there is only a header so we cannot check the content
        if (response.opcode() == ns_o_update && opcode() == ns_o_update) return true;
        
        // the query and response must have the same number of questions
        if (response.questions() != questions()) return false;
        
        // we'll be checking all the questions in the question-section of the response
        // to see if they also appear in the origina query (they should!)
        for (size_t i = 0; i < response.questions(); ++i)
        {
            // get the record from the question-section, and check if the query contains that record
            if (!contains(Question(response, i))) return false;
        }
    
        // there seems to be a match
        return true;
    }
    catch (...)
    {
        // parse error -- we treat this as a no-match
        return false;
    }
}
    
/**
 *  End of namespace
 */
}

