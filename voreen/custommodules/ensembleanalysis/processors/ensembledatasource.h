/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2017 University of Muenster, Germany.                        *
 * Visualization and Computer Graphics Group <http://viscg.uni-muenster.de>        *
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

#ifndef VRN_ENSEMBLEDATASOURCE_H
#define VRN_ENSEMBLEDATASOURCE_H

#include "voreen/core/processors/processor.h"

#include "voreen/core/io/volumeserializerpopulator.h"
#include "voreen/core/ports/genericport.h"
#include "voreen/core/properties/buttonproperty.h"
#include "voreen/core/properties/filedialogproperty.h"
#include "voreen/core/properties/progressproperty.h"
#include "voreen/core/properties/string/stringtableproperty.h"

#include "../ports/ensembledatasetport.h"

namespace voreen {

/**
 * Loads multiple volumes and provides them
 * as VolumeList through its outport.
 */
class VRN_CORE_API EnsembleDataSource : public Processor {

    static const std::string SCALAR_FIELD_NAME;
    static const std::string SIMULATED_TIME_NAME;

public:
    EnsembleDataSource();
    virtual ~EnsembleDataSource();
    virtual Processor* create() const;

    virtual std::string getClassName() const  { return "EnsembleDataSource";    }
    virtual std::string getCategory() const   { return "Input";                 }
    virtual CodeState getCodeState() const    { return CODE_STATE_EXPERIMENTAL; }
    virtual bool usesExpensiveComputation() const { return true;  }

protected:
    virtual void setDescriptions() {
        setDescription("Loads multiple volumes and provides them as VolumeList.");
    }

    void process();
    virtual void initialize();
    virtual void deinitialize();

    void clearEnsembleDataset();
    void buildEnsembleDataset();

    VolumeSerializerPopulator populator_;

    FileDialogProperty ensemblePath_;
    ButtonProperty loadDatasetButton_;
    ProgressProperty runProgress_;
    ProgressProperty timeStepProgress_;
    StringTableProperty loadedRuns_;

    /// The structure of the ensemble data.
    EnsembleDatasetPort outport_;

    static const std::string loggerCat_;
};

} // namespace

#endif