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

#ifndef VRN_ROTATIONALDIRECTIONPROCESSOR_H
#define VRN_ROTATIONALDIRECTIONPROCESSOR_H

#include "voreen/core/processors/processor.h"
#include "voreen/core/ports/volumeport.h"
#include "voreen/core/datastructures/volume/volumeatomic.h"

#include "modules/ensembleanalysis/ports/ensembledatasetport.h"
#include "custommodules/sciviscontest2020/ports/vortexport.h"

namespace voreen {

class VRN_CORE_API RotationalDirectionProcessor : public Processor {
public:

    RotationalDirectionProcessor();

    virtual Processor* create() const;

    virtual std::string getClassName() const { return "RotationalDirectionProcessor"; }

    virtual std::string getCategory() const { return "Volume Processing"; }

    virtual CodeState getCodeState() const { return CODE_STATE_EXPERIMENTAL; }

    static void Process( const VolumeRAM_3xFloat& curl, Vortex& vortex );

protected:

    virtual void setDescriptions()
    {
        setDescription("Calculates the direction of a Vortex, by analysing its coreline and curl");
    }

    virtual void process();

private:

    VolumePort inport_;
    VortexPort inport2_;
    VortexPort outport_;

};

}

#endif