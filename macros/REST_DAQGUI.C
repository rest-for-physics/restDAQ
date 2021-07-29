#include <TApplication.h>
#include <TGButton.h>
#include <TGClient.h>
#include <TGFrame.h>
#include <TGLabel.h>
#include <TObject.h>

#include <iostream>

#include "TRestDAQGUI.h"

void REST_DAQGUI(std::string decodingFile = "") { new TRestDAQGUI(1000, 700, decodingFile); }
