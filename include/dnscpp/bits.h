/**
 *  Bits.h
 * 
 *  DNS messages have certain bits in their header that indicate whether
 *  (for example) DNSSEC-info should be retrieved, and whether the nameserver
 *  should validate the request. This utility class can be uses for
 *  passing such bits
 * 
 *  @author Emiel Bruijntjes <emiel.bruijntjes@copernica.com>
 *  @copyright 2020 Copernica BV
 */

/**
 *  Include guard
 */
#pragma once

/**
 *  Begin of namespace
 */
namespace DNS {

/**
 *  The actual bits, when sent by the client to the server:
 * 
 *      AD:     "please tell me if the data is verified (in case you know)"
 *      CD:     "checking-disabled: if you dont know if data is verified, just give me the data, no need to check it"
 *      DO:     "please also send relevant signatures like RSIG records"
 * 
 *  When sent back in the response from the server to the client:
 * 
 *      AD:     "all records are known to be verified"
 *      CD:     "check was disabled (if AD is not set, it does not mean that the data should not be trusted)"
 * 
 *  @var integer
 */
const int BIT_AD    =   1;
const int BIT_CD    =   2;
const int BIT_DO    =   4;

/**
 *  Class definition
 */
class Bits
{
private:
    /**
     *  The value
     *  @var integer
     */
    int _value;

    /**
     *  Add or remove a bit
     *  @param  bit
     */
    void add(int value) { _value |=  value; }
    void del(int value) { _value &= ~value; }
    
public:
    /**
     *  Constructor
     *  @param  value
     */
    Bits(int value = 0) : _value(value) {}
    
    /**
     *  Destructor
     */
    virtual ~Bits() = default;
    
    /**
     *  Get access to the bits
     *  @return int
     */
    bool AD() const { return _value & BIT_AD; }
    bool CD() const { return _value & BIT_CD; }
    bool DO() const { return _value & BIT_DO; }
    
    /**
     *  Get access to the bits in long notation
     *  @return bool
     */
    bool authentic()        const { return AD(); }
    bool checkingdisabled() const { return CD(); }
    bool dnssec()           const { return DO(); }
    
    /**
     *  Setters
     *  @param  value
     */
    void AD(bool value) { value ? add(BIT_AD) : del(BIT_AD); }
    void CD(bool value) { value ? add(BIT_CD) : del(BIT_CD); }
    void DO(bool value) { value ? add(BIT_DO) : del(BIT_DO); }

    /**
     *  Setters, long notation
     *  @param  value
     */
    void authentic(bool value)        { AD(value); }
    void checkingdisabled(bool value) { CD(value); }
    void dnssec(bool value)           { DO(value); }

};

/**
 *  End of namespace
 */
}

