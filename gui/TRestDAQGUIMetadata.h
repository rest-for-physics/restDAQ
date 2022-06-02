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

#ifndef REST_TRestDAQGUIMetadata
#define REST_TRestDAQGUIMetadata

#include "TRestMetadata.h"

/// This class is meant to hold TRestDAQGUIMetadata for online visualization of the DAQ GUI
/// Note that this class is not meant to be stored in any root file, just for GUI visualization
class TRestDAQGUIMetadata : public TRestMetadata {
    private:

    /// The range where the baseline range will be calculated
    TVector2 fBaseLineRange = TVector2(5, 55);

    /// A parameter to define a minimum signal fluctuation. Measured in sigmas.
    Double_t fSignalThreshold = 2;

    /// Number of bins for the spectra
    Int_t fBinsSpectra = 1000;

    /// Maximum bin for the spectra
    Double_t fSpectraMax = 100000;

    /// Minimum file size to start reading a root file in bytes
    int fMinFileSize = 15*1024;

    /// Name of the readout file if any
    TString fReadoutFile = "";

    /// Name of the readout inside the file
    TString fReadoutName = "";
    

    void Initialize() override;

public:

    inline const TVector2 GetBaselineRange() const{ return fBaseLineRange; }
    inline const Double_t GetSignalThreshold() const { return fSignalThreshold; }
    inline const Int_t GetBinsSpectra() const { return fBinsSpectra; }
    inline const Double_t GetSpetraMax() const { return fSpectraMax; }
    inline const int GetMinFileSize() const { return fMinFileSize; }
    inline const TString GetReadoutFile() const { return fReadoutFile;}
    inline const TString GetReadoutName() const { return fReadoutName;}

    void PrintMetadata() override {
        TRestMetadata::PrintMetadata();

        RESTMetadata << "Baseline range : ( " << fBaseLineRange.X() << " , " << fBaseLineRange.Y() << " ) "
                 << RESTendl;
        RESTMetadata << "Signal threshold : " << fSignalThreshold << " sigmas" << RESTendl;
        RESTMetadata << "Bins Spectra : " << fBinsSpectra << RESTendl;
        RESTMetadata << "Spectra max: " << fSpectraMax << RESTendl;
        RESTMetadata << "Min file size: " << fMinFileSize << " bytes" << RESTendl;
        RESTMetadata << "Readout file/name: " << fReadoutFile<<" "<<fReadoutName << RESTendl;

        TRestMetadata::PrintMetadata();

    }

    TRestDAQGUIMetadata();
    TRestDAQGUIMetadata(const char* configFilename, std::string name = "");
    ~TRestDAQGUIMetadata();

    ClassDefOverride(TRestDAQGUIMetadata, 1);

};
#endif
