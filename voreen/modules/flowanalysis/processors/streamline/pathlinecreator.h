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

#ifndef VRN_PATHLINECREATOR_H
#define VRN_PATHLINECREATOR_H

#include "voreen/core/processors/asynccomputeprocessor.h"

#include "voreen/core/properties/intproperty.h"
#include "voreen/core/properties/buttonproperty.h"
#include "voreen/core/properties/progressproperty.h"
#include "voreen/core/properties/numeric/intervalproperty.h"

#include "voreen/core/ports/volumeport.h"
#include "../../ports/streamlinelistport.h"

namespace voreen {

class SpatioTemporalSampler;

struct PathlineCreatorInput {
    tgt::vec2 absoluteMagnitudeThreshold;
    float velocityUnitConversion;
    float temporalResolution;
    int temporalIntegrationSteps;
    bool enableReseeding;
    int reseedingSteps;
    VolumeRAM::Filter filterMode;
    bool transformVelocites;
    PortDataPointer<VolumeList> flowVolumes;
    const VolumeBase* seedMask;
    std::vector<tgt::vec3> seedPoints;
    std::unique_ptr<StreamlineListBase> output;
};

struct PathlineCreatorOutput {
    std::unique_ptr<StreamlineListBase> pathlines;
};

/**
 * This processor is used to create pathlines from a vec3 volume.
 * It can be used with the StreamlineRenderer3D. At the moment only RAM volumes are supported.
 */
class VRN_CORE_API PathlineCreator : public AsyncComputeProcessor<PathlineCreatorInput, PathlineCreatorOutput> {
public:
    PathlineCreator();

    virtual Processor* create() const { return new PathlineCreator(); }

    virtual std::string getCategory() const { return "Pathline Processing"; }
    virtual std::string getClassName() const { return "PathlineCreator"; }
    virtual CodeState getCodeState() const { return CODE_STATE_TESTING; }

    virtual ComputeInput prepareComputeInput();
    virtual ComputeOutput compute(ComputeInput input, ProgressReporter& progressReporter) const;
    virtual void processComputeOutput(ComputeOutput output);

protected:

    virtual bool isReady() const;
    virtual void adjustPropertiesToInput();

    virtual void setDescriptions() {
        setDescription("This processor is used to create pathlines from a sequence of vec3 volume. The resulting pathlines can be visualized or modified " \
                    "by other processors of the <i>FlowAnalysis</i> module.");
        numSeedPoints_.setDescription("Can be used to determine the number of pathlines, which should be created. It can be used as a performance parameter.");
        seedTime_.setDescription("It is used as debug output to see the current generator. See the next description for more details.");
        enableReseeding_.setDescription("Enables/Disables reseeding of pathlines. If disabled, pathlines are seeded in the initial integration step only.");
        reseedingInterval_.setDescription("Determines the ith integration step, in which pathlines will be reseeded. If set to one, for example, pathlines will be reseeded in every integration step.");
        reseedingIntervalUnitDisplay_.setDescription("Shows the reseeding interval in milli seconds.");
        absoluteMagnitudeThreshold_.setDescription("Flow data points outside the threshold interval will not be used for pathline construction.");
        stopOutsideMask_.setDescription("If set to true, integration will stop if pathlines leave the mask.");
        temporalResolution_.setDescription("Defines the (constant!) time between two consecutive time steps of the input volume list.");
        velocityUnitConversion_.setDescription("Defines the unit of the input velocity volumes");
        temporalIntegrationSteps_.setDescription("Defines the number of integration steps that are executed between two consecutive time steps.");
    }

private:

    struct IntegrationInput {
        float stepSize;
        float velocityUnitConversion;
        tgt::vec2 absoluteMagnitudeThreshold;
        std::function<bool(const tgt::vec3&)> bounds;
    };

    /// Perform a single integration step for the specified pathline.
    bool integrationStep(Streamline& pathline, const SpatioTemporalSampler& sampler, const IntegrationInput& input) const;

    VolumeListPort volumeListInport_;
    VolumePort seedMask_;
    StreamlineListPort pathlineOutport_;

    // seeding
    IntProperty numSeedPoints_;                             ///< number of seed points
    IntProperty seedTime_;                                  ///< seed
    BoolProperty enableReseeding_;
    IntProperty reseedingInterval_;
    FloatProperty reseedingIntervalUnitDisplay_;

    // pathline settings
    FloatIntervalProperty absoluteMagnitudeThreshold_;  ///< only magnitudes in this interval are used
    BoolProperty stopOutsideMask_;                      ///< stop integration if running outside mask
    BoolProperty fitAbsoluteMagnitudeThreshold_;        ///< fit magnitude on input change?
    FloatProperty temporalResolution_;                  ///< (global) temporal resolution between time steps
    OptionProperty<VolumeRAM::Filter> filterMode_;      ///< filtering inside the dataset
    BoolProperty transformVelocities_;                   ///< transform velocities by the volume transformation?

    // debug
    FloatOptionProperty velocityUnitConversion_;
    IntProperty temporalIntegrationSteps_;

    static const std::string loggerCat_;
};

}   // namespace

#endif  // VRN_PATHLINECREATOR_H
