/**
 *  ResolvConf.cpp
 * 
 *  Implementation-file for the resolv.conf parser
 * 
 *  @author Emiel Bruijntjes <emiel.bruijntjes@copernica.com>
 *  @copyright 2020 - 2022 Copernica BV
 */
 
/**
 *  Dependencies
 */
#include "../include/dnscpp/ip.h"
#include "../include/dnscpp/resolvconf.h"
#include <ctype.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include "localdomain.h"

/**
 *  Begin of namespace
 */
namespace DNS {

/**
 *  Helper method calculates the real line-size (minus whitespace)
 *  @param  line        the full line from the file
 *  @param  size        the size _with_ whitespace
 *  @return size_t      the size without whitespace
 */
static size_t linesize(const char *line, size_t size)
{
    // number of characters that can be trimmed
    size_t trim = 0;
    
    // check the trailing characters
    while (size > trim && isspace(line[size-1-trim])) ++trim;
    
    // done
    return size - trim;
}

/**
 *  Size of leading whitespace
 *  @param  line        the line to check   
 *  @param  size        size of the line
 *  @return size_t      number of leading whitespace characters
 */
static size_t whitesize(const char *line, size_t size)
{
    // check the amount of whitespace
    size_t whitespace = 0;
    
    // how many chars can be skipped
    while (size > whitespace && isspace(line[whitespace])) ++whitespace;
    
    // done
    return whitespace;
}

/**
 *  Find first whitespace in a line
 *  @param  line        the line to check
 *  @param  size        size of the line
 *  @return char *      pointer to the first whitespace
 */
static const char *findwhite(const char *line, size_t size)
{
    // check the line
    while (size > 0)
    {
        // do we have a match?
        if (isspace(line[0])) return line;
        
        // prepare for next iteration
        size -= 1; line += 1;
    }
    
    // not found
    return nullptr;
}
    
/**
 *  Check if a line starts with a certain word
 *  @param  line        the line to parse (must already be trimmed)
 *  @param  size        size of the line
 *  @param  required    the word to start with
 *  @return size_t      number of initial bytes that can be stripped
 */
static size_t check(const char *line, size_t size, const char *required)
{
    // line of the word to check
    size_t skip = strlen(required);
    
    // check for the beginning
    if (strncasecmp(line, required, skip) != 0) return 0;
    
    // check the amount of whitespace
    size_t whitespace = whitesize(line+skip, size-skip);
    
    // if there was no whitespace at all the option does not have a value, or it
    // is not skipped with whitespace from the value, we treat this as a no-match
    if (whitespace == 0) return 0;
    
    // we now know the total size
    return skip + whitespace;
}

/**
 *  Constructor
 *  @param  filename        the file to parse
 *  @param  strict          run in strict mode (do not allow unsupported options)
 *  @throws std::runtime_error
 */
ResolvConf::ResolvConf(const char *filename, bool strict)
{
    // open the file for reading
    std::ifstream stream(filename);
    
    // file should be open by now
    if (!stream.is_open()) throw std::runtime_error(std::string(filename) + ": failed to open file");
    
    // keep readling lines until the end
    while (!stream.eof())
    {
        // catch exceptions to reformat them
        try
        {
            // we are going to read lines, we need a local variable for that
            std::string line;
        
            // go read the line
            getline(stream, line);
            
            // remove trailing whitespace
            line.resize(linesize(line.data(), line.size()));
            
            // parse the line
            parse(line.data(), line.size());
        }
        catch (const std::runtime_error &error)
        {
            // ignore the error if not running in strict mode, otherwise reformat it
            if (strict) throw std::runtime_error(std::string(filename) + ": " + error.what());
        }
    }

    // if there was no explicit search-path specified, we will add the own domain to it
    if (!_searchpaths.empty()) return;
    
    // find the local domain
    LocalDomain localdomain;

    // if the localdomain is the root-domain, we do not add it to the search path, because deeper 
    // down in the DNS-CPP library it is then more efficient to start the actual query right away, 
    // instead of the loop to try all domains in the search-list
    if (strlen(localdomain) == 0) return;
    
    // add to the paths
    _searchpaths.emplace_back(localdomain);
}

/**
 *  Helper method to parse lines
 *  @param  line        the line to parse (must already be trimmed)
 *  @param  size        size of the line
 *  @throws std::runtime_error
 */
void ResolvConf::parse(const char *line, size_t size)
{
    // skip empty lines or lines that are commented out
    if (size == 0 || line[0] == ';' || line[0] == '#') return;
    
    // helper variable
    size_t skip = 0;
     
    // check the instruction
    if ((skip = check(line, size, "nameserver")) > 0) return nameserver(line+skip, size-skip);
    if ((skip = check(line, size, "options"))    > 0) return options(line+skip, size-skip);
    if ((skip = check(line, size, "domain"))     > 0) return domain(line+skip, size-skip);
    if ((skip = check(line, size, "search"))     > 0) return search(line+skip, size-skip);
    
    // invalid line
    throw std::runtime_error(std::string("unrecognized: ") + line);
}
    
/**
 *  Parse a line holding a nameserver
 *  @param  line        the value to parse
 *  @param  size        size of the line
 *  @throws std::runtime_error
 */
void ResolvConf::nameserver(const char *line, size_t size)
{
    // add a nameserver
    _nameservers.emplace_back(line);
}

/**
 *  Add the local domain
 *  @param  line        the value to parse
 *  @param  size        size of the line
 *  @throws std::runtime_error
 */
void ResolvConf::domain(const char *line, size_t size)
{
    // report error
    throw std::runtime_error(std::string("not implemented: domain ") + line);
}

/**
 *  Add a search path
 *  @param  line        the value to parse
 *  @param  size        size of the line
 *  @throws std::runtime_error
 */
void ResolvConf::search(const char *line, size_t size)
{
    // we only remember the last entry, so we remove potential previous entries
    _searchpaths.clear();

    // keep looking for paths
    while (size > 0)
    {
        // find an end-marker (whitespace or eos)
        const char *end = findwhite(line, size);

        // are we at the end? the last element
        if (end == nullptr) return (void)_searchpaths.emplace_back(line, size);

        // size of the part that we found
        size_t partsize = end - line;

        // if we're not at the end, we add a part
        _searchpaths.emplace_back(line, partsize);

        // prepare for calculating leading whitespace
        line += partsize; size -= partsize;

        // calculate initial whitespace
        size_t white = whitesize(line, size);

        // prepare more for next iteragtion
        line += white; size -= white;
    }
}

/**
 *  Add an option
 *  @param  line        the value to parse
 *  @param  size        size of the line
 *  @throws std::runtime_error
 */
void ResolvConf::options(const char *line, size_t size)
{
    // define the start and the line
    const char *start = line;
    const char *space = strchr(start, ' ');

    // iterate over the options, separate by spaces
    while (start)
    {
        // process the option
        option(start, space ? space - start : size - (start - line));

        // if there was no space, we're reached the end of the line, and we're done
        if (!space) return;

        // find the next space
        start = space + 1;
        space = strchr(start, ' ');
    }
}

/**
 *  Add an option
 *  @param  option  
 *  @param  size
 */
void ResolvConf::option(const char *option, size_t size)
{
    // if the option is empty, there is nothing to check
    if (size == 0) return;

    // check if this is the rotate option
    if (strncmp(option, "rotate", 7) == 0) _rotate = true;
    
    // maybe this is the timeout option, needs to be capped to 30 (per the conf)
    else if (strncmp(option, "timeout:", 8) == 0) _timeout = std::min(30, atoi(option + 8));

    // maybe this is the attempts option? parse it and cap it to 5.
    else if (strncmp(option, "attempts:", 9) == 0) _attempts = std::min(5, atoi(option + 9));

    // may the ndots setting? cap it to 15
    else if (strncmp(option, "ndots:", 6) == 0) _ndots = std::min(15, atoi(option+6));

    // unknown option...
}
  
/**
 *  End of namespace
 */
}

