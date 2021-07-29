#include <iostream>

#include <TGClient.h>
#include <TGButton.h>
#include <TGFrame.h>
#include <TGLabel.h>
#include <TObject.h>
#include <TApplication.h> 

#include "TRestDAQGUI.h"


void REST_DAQGUI(std::string decodingFile="") {

new TRestDAQGUI (1000,700,decodingFile);

}

