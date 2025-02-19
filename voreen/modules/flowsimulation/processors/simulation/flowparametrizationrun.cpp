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

#include "flowparametrizationrun.h"

#include "../../utils/serializationhelper.h"

#define PARAMETER_DISCRETIZATION_BEGIN(PROPERTY, TYPE) \
    int samples ## PROPERTY = samples_.get(); \
    if(PROPERTY ## _.get().x == PROPERTY ## _.get().y) { \
        samples ## PROPERTY = 1; \
    } \
    std::string tmp = name; \
    for(int PROPERTY ## i = 0; PROPERTY ## i < samples ## PROPERTY; PROPERTY ## i++) { \
        TYPE PROPERTY = PROPERTY ## _.get().x; \
        std::string name = tmp; \
        if(samples ## PROPERTY > 1) { \
            PROPERTY += (PROPERTY ## _.get().y - PROPERTY ## _.get().x) \
                            * PROPERTY ## i / (samples ## PROPERTY - 1); \
            name += static_cast<char>('A' + PROPERTY ## i); \
        } \
//std::string name = tmp + std::string(#PROPERTY).substr(0, 3) + "=" + std::to_string(PROPERTY);

#define PARAMETER_DISCRETIZATION_END }


namespace voreen {

const std::string FlowParametrizationRun::loggerCat_("voreen.flowsimulation.FlowParametrizationRun");

FlowParametrizationRun::FlowParametrizationRun()
    : Processor()
    , inport_(Port::INPORT, "inport", "Parameter Inport")
    , outport_(Port::OUTPORT, "outport", "Parameter Inport")
    , parametrizationName_("parametrizationName", "Parametrization Name", "test_parametrization")
    , spatialResolution_("spatialResolution", "Spatial Resolution", 32, 8, 512)
    , relaxationTime_("relaxationTime", "Relaxation Time", 1.0f, 0.5f, 4.0f)
    , characteristicLength_("characteristicLength", "Characteristic Length (mm)", 10.0f, 0.1f, 10000.0f)
    , characteristicVelocity_("characteristicVelocity", "Characteristic Velocity (m/s)", 1.0f, 0.0f, 10.0f)
    , fluid_("fluid", "Fluid")
    , viscosity_("viscosity", "Kinematic Viscosity (x10^(-6) m^2/s)", 3.5, 3, 4)
    , density_("density", "Density (kg/m^3)", 1000.0f, 1000.0f, 1100.0f)
    , turbulenceModel_("turbulenceModel", "Turbulence Model")
    , smagorinskyConstant_("smagorinskyConstant", "Smagorinsky Constant", 0.1f, 0.1f, 5.0f)
    , wallBoundaryCondition_("wallBoundaryCondition", "Wall Boundary Condition")
    , latticePerturbation_("latticePerturbation", "Lattice Perturbation", false)
    , inletVelocityMultiplier_("inletVelocityMultiplier", "Inlet Velocity Multiplier", 1.0f, 0.1f, 10.0f)
    , samples_("samples", "Samples", 3, 1, 26)
    , addParametrization_("addParametrizations", "Add Parametrizations")
    , removeParametrization_("removeParametrization", "Remove Parametrization")
    , clearParametrizations_("clearParametrizations", "Clear Parametrizations")
    , parametrizations_("parametrizations", "Parametrizations", 14, Processor::VALID)
    , addInvalidParametrizations_("addInvalidParametrizations", "Add invalid Parametrizations", false)
{
    addPort(inport_);
    addPort(outport_);

    addProperty(spatialResolution_);
        spatialResolution_.setGroupID("numerical");
    addProperty(relaxationTime_);
        relaxationTime_.setNumDecimals(3);
        relaxationTime_.setGroupID("numerical");
    addProperty(characteristicLength_);
        characteristicLength_.setGroupID("numerical");
    addProperty(characteristicVelocity_);
        characteristicVelocity_.setNumDecimals(3);
        characteristicVelocity_.setGroupID("numerical");
    addProperty(turbulenceModel_);
        turbulenceModel_.addOption("smagorinsky", "Smagorinsky", FTM_SMAGORINSKY);
        turbulenceModel_.addOption("smagorinskyShearImproved", "Shaer Improved Smagorinsky", FTM_SMAGORINSKY_SHEAR_IMPROVED);
        turbulenceModel_.addOption("smagorinskyConsistent", "Consistent Smagorinsky", FTM_SMAGORINSKY_CONSISTENT);
        turbulenceModel_.addOption("smagorinskyConsistentStrain", "Strain Consistent Smagorinsky", FTM_SMAGORINSKY_CONSISTENT_STRAIN);
        turbulenceModel_.addOption("BGK", "BGK", FTM_BGK);
        turbulenceModel_.setGroupID("numerical");
    addProperty(smagorinskyConstant_);
        smagorinskyConstant_.setNumDecimals(3);
        smagorinskyConstant_.setGroupID("numerical");
    addProperty(wallBoundaryCondition_);
        wallBoundaryCondition_.addOption("bouzidi", "Bouzidi", FBC_BOUZIDI);
        wallBoundaryCondition_.addOption("bounceBack", "Bounce-Back", FBC_BOUNCE_BACK);
        wallBoundaryCondition_.setGroupID("numerical");
    addProperty(latticePerturbation_);
        latticePerturbation_.setGroupID("numerical");
    setPropertyGroupGuiName("numerical", "Numerical Parameters");

    addProperty(fluid_);
        ON_CHANGE(fluid_, FlowParametrizationRun, fluidChanged);
        fluid_.addOption("arbitrary", "Arbitrary", FLUID_ARBITRARY);
        fluid_.addOption("water", "Water", FLUID_WATER);
        fluid_.addOption("blood", "Blood", FLUID_BLOOD);
        fluid_.setGroupID("fluid");
        fluidChanged(); // Init proper values.
    addProperty(viscosity_);
        viscosity_.setNumDecimals(4);
        viscosity_.setGroupID("fluid");
    addProperty(density_);
        density_.setNumDecimals(0);
        density_.setGroupID("fluid");
    setPropertyGroupGuiName("fluid", "Fluid Parameters");

    addProperty(inletVelocityMultiplier_);
        inletVelocityMultiplier_.setGroupID("inlet");
    setPropertyGroupGuiName("inlet", "Inlet Parameters");

    addProperty(parametrizationName_);
        parametrizationName_.setGroupID("general");
    addProperty(samples_);
        samples_.setGroupID("general");
    addProperty(addInvalidParametrizations_);
        addInvalidParametrizations_.setGroupID("general");
    addProperty(addParametrization_);
        ON_CHANGE(addParametrization_, FlowParametrizationRun, addParametrizations);
        addParametrization_.setGroupID("general");
    addProperty(removeParametrization_);
        ON_CHANGE(removeParametrization_, FlowParametrizationRun, removeParametrization);
        removeParametrization_.setGroupID("general");
    addProperty(clearParametrizations_);
        ON_CHANGE(clearParametrizations_, FlowParametrizationRun, clearParametrizations);
        clearParametrizations_.setGroupID("general");
    setPropertyGroupGuiName("general", "Edit Run Parameters");

    addProperty(parametrizations_);
    std::string columnLabels[] = {"Valid", "Name", "Re", "N", "τ", "l", "U", "ν", "ρ", "Model", "Cs", "Bound. Cond.", "Mul.", "Pert."};
    for(int i=0; i<parametrizations_.getNumColumns(); i++) {
        parametrizations_.setColumnLabel(i, columnLabels[i]);
    }
}

void FlowParametrizationRun::fluidChanged() {
    switch(fluid_.getValue()) {
    case FLUID_ARBITRARY:
        viscosity_.setMinValue(0.1f);
        viscosity_.setMaxValue(100.0f);
        viscosity_.set(1.0f);
        density_.setMinValue(0.1f);
        density_.setMaxValue(10000.0f);
        density_.set(1000.0f);
        break;
    case FLUID_WATER:
        viscosity_.setMinValue(0.79722f);
        viscosity_.setMaxValue(1.35f);
        viscosity_.set(1.0016f); // at room temperature
        density_.setMinValue(988.1f);
        density_.setMaxValue(1000.0f);
        density_.set(998.21f); // at room temperature
        break;
    case FLUID_BLOOD:
        viscosity_.setMinValue(3.0f);
        viscosity_.setMaxValue(4.0f);
        viscosity_.set(4.0f); // literature value
        density_.setMinValue(1043.0f);
        density_.setMaxValue(1057.0f);
        density_.set(1055.0f); // literature value
        break;
    default:
        tgtAssert(false, "Unhandled fluid");
        break;
    }
}

void FlowParametrizationRun::addParametrizations() {

    std::string name = parametrizationName_.get();
    for(const Parameters& params : flowParameters_) {
        if(params.name_.find(name) != std::string::npos) {
            VoreenApplication::app()->showMessageBox("Warning", "Already parametrization added with the same prefix");
            LWARNING("Already parametrization with prefix " << name);
            return;
        }
    }

    // Replace spaces with underscores (spaces might cause problems as the names are treated as paths later).
    std::replace(name.begin(), name.end(), ' ', '_');

    PARAMETER_DISCRETIZATION_BEGIN(spatialResolution, int)
    PARAMETER_DISCRETIZATION_BEGIN(relaxationTime, float)
    PARAMETER_DISCRETIZATION_BEGIN(viscosity, float)
    PARAMETER_DISCRETIZATION_BEGIN(density, float)
    PARAMETER_DISCRETIZATION_BEGIN(smagorinskyConstant, float)
    PARAMETER_DISCRETIZATION_BEGIN(inletVelocityMultiplier, float)

    Parameters parameters;
    parameters.name_ = name;
    parameters.spatialResolution_ = spatialResolution;
    parameters.relaxationTime_ = relaxationTime;
    parameters.characteristicLength_ = characteristicLength_.get() * 0.001f; // [mm] to [m]
    parameters.characteristicVelocity_ = characteristicVelocity_.get();
    parameters.viscosity_ = viscosity * 10e-6f; // Due to interface value range.
    parameters.density_ = density;
    parameters.turbulenceModel_ = turbulenceModel_.getValue();
    parameters.smagorinskyConstant_ = smagorinskyConstant;
    parameters.wallBoundaryCondition_ = wallBoundaryCondition_.getValue();
    parameters.inletVelocityMultiplier_ = inletVelocityMultiplier;
    parameters.latticePerturbation_ = latticePerturbation_.get();
    flowParameters_.emplace_back(parameters);

    std::vector<std::string> row;
    row.push_back(parameters.isValid() ? "✓" : "✗");
    row.push_back(parameters.name_);
    row.push_back(std::to_string(parameters.getReynoldsNumber()));
    row.push_back(std::to_string(parameters.spatialResolution_));
    row.push_back(std::to_string(parameters.relaxationTime_));
    row.push_back(std::to_string(parameters.characteristicLength_));
    row.push_back(std::to_string(parameters.characteristicVelocity_));
    row.push_back(std::to_string(parameters.viscosity_));
    row.push_back(std::to_string(parameters.density_));
    row.push_back(turbulenceModel_.getDescription());
    row.push_back(std::to_string(parameters.smagorinskyConstant_));
    row.push_back(wallBoundaryCondition_.getDescription());
    row.push_back(std::to_string(parameters.inletVelocityMultiplier_));
    row.push_back(std::to_string(parameters.latticePerturbation_));
    parametrizations_.addRow(row);

    PARAMETER_DISCRETIZATION_END
    PARAMETER_DISCRETIZATION_END
    PARAMETER_DISCRETIZATION_END
    PARAMETER_DISCRETIZATION_END
    PARAMETER_DISCRETIZATION_END
    PARAMETER_DISCRETIZATION_END
}

void FlowParametrizationRun::removeParametrization() {
    if(parametrizations_.getNumRows() > 0 && parametrizations_.getSelectedRowIndex() >= 0) {
        flowParameters_.erase(flowParameters_.begin() + parametrizations_.getSelectedRowIndex());
        parametrizations_.removeRow(parametrizations_.getSelectedRowIndex());
    }
    else {
        VoreenApplication::app()->showMessageBox("Warning", "No parametrization selected");
        LWARNING("No parametrization selected");
    }
}

void FlowParametrizationRun::clearParametrizations() {
    parametrizations_.reset();
    flowParameters_.clear();
}

void FlowParametrizationRun::process() {

    FlowSimulationConfig* config = new FlowSimulationConfig(*inport_.getData());

    for (const Parameters& parameters : flowParameters_) {
        if(addInvalidParametrizations_.get() || parameters.isValid()) {
            config->addFlowParameterSet(parameters);
        }
    }

    outport_.setData(config);
}

void FlowParametrizationRun::adjustPropertiesToInput() {
    if(!inport_.hasData()) {
        return;
    }

    float minCharacteristicVelocity = 0.0f;
    for(const FlowIndicator& indicator : inport_.getData()->getFlowIndicators()) {
        minCharacteristicVelocity = std::max(minCharacteristicVelocity, indicator.velocityCurve_.getMaxVelocity());
    }
    characteristicVelocity_.setMinValue(minCharacteristicVelocity);
}

void FlowParametrizationRun::serialize(Serializer& s) const {
    Processor::serialize(s);
    serializeVector<ParametersSerializable, Parameters>(s, "flowParameters", flowParameters_);
}

void FlowParametrizationRun::deserialize(Deserializer& s) {
    Processor::deserialize(s);
    deserializeVector<ParametersSerializable, Parameters>(s, "flowParameters", flowParameters_);
}

}   // namespace
