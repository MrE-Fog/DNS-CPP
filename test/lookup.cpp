/**
 *  Lookup.cpp
 * 
 *  Test program for DNS lookups
 * 
 *  @author Emiel Bruijntjes <emiel.bruijntjes@copernica.com>
 *  @copyright 2020 Copernica BV
 */
 
/**
 *  Dependencies
 */
#include <ev.h>
#include <dnscpp.h>
#include <dnscpp/libev.h>
#include <iostream>

/**
 *  Helper function to convert type
 *  @param  input
 *  @return int
 *  @throws std::runtime_error
 */
static ns_type convert(const char *type)
{
    // check all types
    if (strcasecmp(type, "a")       == 0) return ns_t_a;
    if (strcasecmp(type, "aaaa")    == 0) return ns_t_aaaa;
    if (strcasecmp(type, "mx")      == 0) return ns_t_mx;
    if (strcasecmp(type, "txt")     == 0) return ns_t_txt;
    if (strcasecmp(type, "cname")   == 0) return ns_t_cname;
    if (strcasecmp(type, "ptr")     == 0) return ns_t_ptr;
    if (strcasecmp(type, "caa")     == 0) return ns_t_caa;
    if (strcasecmp(type, "ns")      == 0) return ns_t_ns;
    if (strcasecmp(type, "ptr")     == 0) return ns_t_ptr;
    
    // invalid type
    throw std::runtime_error(std::string("unknown record type ") + type);
}

/**
 *  Class to handle responses
 */
class MyHandler : public DNS::Handler
{
public:
    /**
     *  Method that is called when an operation times out.
     *  @param  operation       the operation that timed out
     */
    virtual void onTimeout(const DNS::Operation *operation) override
    {
        std::cout << "timeout" << std::endl;
    }
    
    /**
     *  Method that is called when a raw response is received.
     *  @param  operation       the operation that finished
     *  @param  response        the received response
     */
    virtual void onReceived(const DNS::Operation *operation, const DNS::Response &response) override
    {
        // opcode status and id
        std::cout << ";; Opcode: " << response.opcode() << ", status: " << response.rcode() << ", id: " << response.id() << std::endl;
        
        // flags, number of sections
        std::cout << ";; Flags: " 
            << (response.question() ? "qr " : "")
            << (response.authoratative() ? "aa " : "")
            << (response.truncated() ? "tc " : "")
            << (response.recursiondesired() ? "rd " : "")
            << (response.recursionavailable() ? "ra " : "")
            << "; QUERY: " << response.records(ns_s_qd)
            << ", ANSWER: " << response.records(ns_s_an)
            << ", AUTHORITY: " << response.records(ns_s_ns)
            << ", ADDITIONAL: " << response.records(ns_s_ar) << std::endl;
            
        // print the sections
        printsection(response, "QUESTION", ns_s_qd);
        printsection(response, "ANSWER", ns_s_an);
        printsection(response, "AUTHORITY", ns_s_ns);
    }

private:
    /**
     *  Helper method to print a section    
     *  @param  response
     *  @param  name
     *  @param  section
     */
    static void printsection(const DNS::Response &response, const char *name, ns_sect section)
    {
        // number of records
        size_t count = response.records(section);
        
        // do nothing if section is empty
        if (count == 0) return;
        
        // output header
        std::cout << ";; " << name << " SECTION" << std::endl;
        
        // parse all records
        for (size_t i = 0; i < count; ++i)
        {
            // the record
            DNS::Record record(response, section, i);
            
            // output something
            std::cout << record.name() << "\tttl:" << record.ttl() << "\tclass:" << record.dnsclass() << "\ttype:" << record.type() << "\t";
            
            try
            {
                // check the type
                if (section != ns_s_qd) switch (record.type()) {
                case ns_t_a:        std::cout << DNS::A(response, record).ip(); break;
                case ns_t_aaaa:     std::cout << DNS::AAAA(response, record).ip(); break;
                case ns_t_mx:       std::cout << DNS::MX(response, record).priority() << " " << DNS::MX(response, record).hostname(); break;
                case ns_t_cname:    std::cout << DNS::CNAME(response, record).target(); break;
                case ns_t_txt:      std::cout << DNS::TXT(response, record).data(); break;
                case ns_t_ns:       std::cout << DNS::NS(response, record).nameserver(); break;
                case ns_t_ptr:      std::cout << DNS::PTR(response, record).target(); break;
                case ns_t_soa:      { DNS::SOA soa(response, record); std::cout << soa.nameserver() << " " << soa.email() << " " << soa.serial() << " " << soa.interval() << " " << soa.retry() << " " << soa.expire() << " " << soa.minimum(); } break;
    //            case ns_t_caa:      std::cout << DNS::CAA(response, record); break;
                default:            std::cout << "unknown"; break;
                }
            
            }
            catch (const std::runtime_error &error)
            {
                std::cout << "parse error " << error.what();
            }
            
            // newline
            std::cout << std::endl;
        }
        
        // mark end of section
        std::cout << std::endl;
    }
};

/** Main procedure
 *  @param  argc
 *  @param  argv
 *  @return int
 */
int main(int argc, const char *argv[])
{
    // avoid exceptions
    try
    {
        // the event loop
        struct ev_loop *loop = EV_DEFAULT;
    
        // wrap the loop to make it accessible by dns-cpp
        DNS::LibEv myloop(loop);
    
        // create a dns context
        DNS::Context context(&myloop);
        
        // make sure to have a nameserver
        context.nameserver(DNS::Ip("8.8.8.8"));

        // check usage
        if (argc != 3) throw std::runtime_error(std::string("usage: ") + argv[0] + " type value");
        
        // the type
        ns_type type = convert(argv[1]);
        const char *value = argv[2];
        
        // object that will handle the call
        MyHandler handler;
        
        // do the lookup
        context.query(value, type, &handler);
        
        // run the event loop
        ev_run(loop);
        
        // success
        return 0;
    }
    catch (const std::runtime_error &error)
    {
        // report error
        std::cerr << error.what() << std::endl;
        
        // exit code
        return -1;
    }
}
