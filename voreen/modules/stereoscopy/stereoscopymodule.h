/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2024 University of Muenster, Germany,                        *
 * Department of Computer Science.                                                 *
 * For a list of authors please refer to the file "CREDITS.txt".                   *
 *                                                                                 *
 * This file is part of the Voreen software package. Voreen is free software:      *
 * you can redistribute it and/or modify it under the terms of the GNU General     *
 * Public License version 2 as published by the Free Software Foundation.          *
 *                                                                                 *
 * Voreen is distributed in the hope that it will be useful, but WITHOUT ANY       *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR   *
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.      *
 *                                                                                 *
 * You should have received a copy of the GNU General Public License in the file   *
 * "LICENSE.txt" along with this file. If not, see <http://www.gnu.org/licenses/>. *
 *                                                                                 *
 * For non-commercial academic use see the license exception specified in the file *
 * "LICENSE-academic.txt". To get information about commercial licensing please    *
 * contact the authors.                                                            *
 *                                                                                 *
 ***********************************************************************************/

#ifndef VRN_STEREOSCOPYMODULE_H
#define VRN_STEREOSCOPYMODULE_H

#include "voreen/core/voreenmodule.h"

namespace voreen {

class StereoscopyModule: public VoreenModule {

public:
    StereoscopyModule(const std::string& modulePath);

    virtual std::string getDescription() const {
        return "This module contains all tools for stereoscopic rendering. "\
               "Basically the StereoCanvasRenderer a.k.a. StereoCanvas. "\
               "Supported are different stereoscopy modes: anaglyph, autostereoscopic and split-screen."\
               "More information on <a href=\"http://voreen.uni-muenster.de/?q=node/50\" >voreen.uni-muenster.de</a>";
    }
};

} // namespace

#endif //VRN_STEREOSCOPYMODULE_H
