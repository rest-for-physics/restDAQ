/*************************************************************************
 * This file is part of the REST software framework.                     *
 *                                                                       *
 * Copyright (C) 2016 GIFNA/TREX (University of Zaragoza)                *
 * For more information see https://gifna.unizar.es/trex                 *
 *                                                                       *
 * REST is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * REST is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have a copy of the GNU General Public License along with   *
 * REST in $REST_PATH/LICENSE.                                           *
 * If not, see https://www.gnu.org/licenses/.                            *
 * For the list of contributors see $REST_PATH/CREDITS.                  *
 *************************************************************************/

/////////////////////////////////////////////////////////////////////////
/// TRestDAQGUIMetadata holds the required configuration to launch
/// a Grafical User Interface for data acquisition aka TRestDAQGUI.
/// The default initialization is used in case no TRestDAQGUIMetadata
/// while a TRestDAQGUI is instanciated.
///
/// ### Parameters
/// * **baseLineRange**: Range where the baseline range will be calculated
/// in the online analysis
/// * **signalThreshold**: Minimum number of sigmas to consider that a signal
/// is above threshold.
/// * **binsSpectra**: Number of bins for the spectrum
/// * **spectraMax**: Maximum value in the spectrum
/// * **minFileSize**: Minimum file size to start reading a root file (bytes)
/// * **readoutFile**: File name (root) where the readout is stored
/// * **readoutName**: Name of the readout stored in the root file.
///
/// ### Examples
/// Give examples of usage and RML descriptions that can be tested.
/// \code
///  <TRestManager>
///    <TRestDAQGUIMetadata name="GUI Metadata" title="GUI Metadata" verboseLevel="info">
///        <parameter name="baseLineRange" value="(50,120)"/>
///        <parameter name="signalThreshold" value="2.0"/>
///        <parameter name="binsSpectra" value="1000"/>
///        <parameter name="spectraMax" value="100000"/>
///        <parameter name="minFileSize" value="15360"/>
///        <parameter name="readoutFile" value="DummyReadout.root"/>
///        <parameter name="readoutName" value="dummy"/>
///    </TRestDAQGUIMetadata>
///  </TRestManager>
/// \endcode
///
///
///----------------------------------------------------------------------
///
/// REST-for-Physics - Software for Rare Event Searches Toolkit
///
/// History of developments:
///
/// 2022-June: First implementation of TRestDAQGUIMetadata
/// JuanAn Garcia
///
/// \class TRestDAQGUIMetadata
/// \author: JuanAn Garcia e-mail: juanangp@unizar.es
///
/// <hr>
///

#include "TRestDAQGUIMetadata.h"

ClassImp(TRestDAQGUIMetadata);

///////////////////////////////////////////////
/// \brief Default constructor
///
TRestDAQGUIMetadata::TRestDAQGUIMetadata() { Initialize(); }

/////////////////////////////////////////////
/// \brief Constructor loading data from a config file
///
TRestDAQGUIMetadata::TRestDAQGUIMetadata(const char* configFilename, std::string name) : TRestMetadata(configFilename) {
    LoadConfigFromFile(fConfigFileName, name);

    if (GetVerboseLevel() >= TRestStringOutput::REST_Verbose_Level::REST_Info) PrintMetadata();
}

///////////////////////////////////////////////
/// \brief Default destructor
///
TRestDAQGUIMetadata::~TRestDAQGUIMetadata() {}

///////////////////////////////////////////////
/// \brief Function to initialize input/output event members and define
/// the section name
///
void TRestDAQGUIMetadata::Initialize() { SetSectionName(this->ClassName()); }
