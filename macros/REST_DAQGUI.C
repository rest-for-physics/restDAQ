
#include "TRestDAQGUI.h"

void REST_DAQGUI(const std::string &inputRMLFile) {

  TRestDAQGUIMetadata *mt = new TRestDAQGUIMetadata(inputRMLFile.c_str());
  new TRestDAQGUI(1000, 700, mt); 
 
}
