
#ifndef __TREST_DAQ_EXCEPTION__
#define __TREST_DAQ_EXCEPTION__

#include<string.h>


class TRESTDAQException : public std::exception {
  public:
    TRESTDAQException(const std::string& msg) : m_msg(msg){
    }

   ~TRESTDAQException(){ }

   virtual const char* what() const throw () {
        return m_msg.c_str();
   }
  private:
   const std::string m_msg;
};

#endif

