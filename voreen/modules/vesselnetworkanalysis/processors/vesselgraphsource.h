/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2018 University of Muenster, Germany,                        *
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

#ifndef VRN_VESSELGRAPHSOURCE_H
#define VRN_VESSELGRAPHSOURCE_H

#include "voreen/core/processors/processor.h"

#include "../ports/vesselgraphport.h"
#include "voreen/core/properties/filedialogproperty.h"
#include "voreen/core/properties/buttonproperty.h"

namespace voreen {

class VesselGraphSource : public Processor {
public:
    VesselGraphSource();
    virtual ~VesselGraphSource();
    virtual std::string getCategory() const { return "Input"; }
    virtual std::string getClassName() const { return "VesselGraphSource"; }
    virtual CodeState getCodeState() const { return Processor::CODE_STATE_TESTING; }
    virtual Processor* create() const { return new VesselGraphSource(); }

protected:
    virtual void setDescriptions() {
        setDescription("This processor can be used to load vessel graph files that have been previously saved using <b>VesselGraphSave</b>. "
                "Vesselgraphs are serialized in a custom (but simple) json format that is gzip-compressed before writing it to disk."
                );
        graphFilePath_.setDescription("Path to the *.vvg.gz file to be loaded.");
    }

    virtual void process();

    VesselGraphPort outport_;

    // properties
    FileDialogProperty graphFilePath_;
    ButtonProperty reload_;


    static const std::string loggerCat_;
};

} // namespace voreen
#endif // VRN_VESSELGRAPHSOURCE_H