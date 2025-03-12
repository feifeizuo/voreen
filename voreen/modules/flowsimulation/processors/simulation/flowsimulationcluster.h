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

#ifndef VRN_FLOWSIMULATIONCLUSTER_H
#define VRN_FLOWSIMULATIONCLUSTER_H

#include "voreen/core/processors/asynccomputeprocessor.h"

#include "voreen/core/ports/geometryport.h"
#include "voreen/core/ports/volumeport.h"
#include "../../ports/flowsimulationconfigport.h"

#include "voreen/core/properties/buttonproperty.h"
#include "voreen/core/properties/stringproperty.h"
#include "voreen/core/properties/optionproperty.h"
#include "voreen/core/properties/progressproperty.h"
#include "voreen/core/properties/temppathproperty.h"

namespace voreen {

class ExecutorProcess;


/**
 * This processor performs simulations on the PALMAII cluster at WWU Muenster using a parameter set and as input.
 * This processor assumes:
 *     - on the cluster, there is a folder in home called 'OpenLB/<tool-chain>/simulations'
 *     - the result will be saved to /scratch/tmp/<user>/simulations/<simulation_name>/<run_name>
 */
class VRN_CORE_API FlowSimulationCluster : public Processor {
public:
    FlowSimulationCluster();
    virtual ~FlowSimulationCluster();
    virtual Processor* create() const         { return new FlowSimulationCluster();    }

    virtual std::string getClassName() const  { return "FlowSimulationCluster";        }
    virtual std::string getCategory() const   { return "Simulation";                   }
    virtual CodeState getCodeState() const    { return CODE_STATE_EXPERIMENTAL;        }

    virtual bool isReady() const;
    virtual void process();

protected:
    virtual void setDescriptions() {
        setDescription("This processor performs simulations on the PALMAII cluster at WWU Muenster using a parameter set and as input. "
                       "The processor assumes that on the cluster, there is a folder '<simulation-path>/OpenLB/<tool-chain>/simulations'. "
                       "The simulation artifacts will be stored to and fetched from '/scratch/tmp/<user>/simulations/<simulation_name>/<run_name>'");
    }

private:

    void threadsStopped();
    void workloadManagerChanged();
    void refreshClusterCode();
    void enqueueSimulations();
    void fetchResults();

    void stepCopyGeometryData(FlowSimulationConfig& config, const std::string& simulationPathSource);
    void stepCopyMeasurementData(const VolumeList* volumeList, FlowSimulationConfig& config, const std::string& simulationPathSource);
    void stepCreateSimulationConfigs(FlowSimulationConfig& config, const std::string& simulationPathSource);

    void runLocal(FlowSimulationConfig& config, std::string simulationPathSource, std::string simulationPathDest);
    void runCluster(FlowSimulationConfig& config, std::string simulationPathSource, std::string simulationPathDest);

    std::string generateCompileScript() const;
    std::string generateEnqueueScript(const std::string& parametrizationPath) const;
    std::string generateSubmissionScript(const std::string& parametrizationName) const;

    std::map<float, std::string> checkAndConvertVolumeList(const VolumeList* volumes, tgt::mat4 transformation, const std::string& simulationPathSource, const std::string& volumeType) const;

    GeometryPort geometryDataPort_;
    VolumeListPort geometryVolumeDataPort_;
    VolumeListPort measuredDataPort_;
    FlowSimulationConfigPort configPort_;

    StringOptionProperty workloadManager_;
    BoolProperty useLocalInstance_;
    FileDialogProperty localInstancePath_;
    BoolProperty detachProcesses_;
    BoolProperty overwriteExistingConfig_;
    ButtonProperty stopProcesses_;

    StringProperty username_;
    StringProperty emailAddress_;
    StringProperty clusterAddress_;
    StringProperty programPath_;
    StringProperty dataPath_;
    StringOptionProperty simulationType_;

    StringOptionProperty configToolchain_;
    IntProperty configNodes_;
    IntProperty configNumGPUs_;
    IntProperty configTasksPerNode_;
    IntProperty configCPUsPerTask_;
    IntProperty configMemory_;
    IntProperty configTimeDays_;
    IntProperty configTimeQuarters_;
    StringOptionProperty configPartition_;

    FileDialogProperty simulationResults_;
    TempPathProperty uploadDataPath_;
    ButtonProperty refreshClusterCode_;
    BoolProperty deleteOnDownload_;
    ButtonProperty triggerEnqueueSimulations_;
    ButtonProperty triggerFetchResults_;
    ProgressProperty progress_;

    std::deque<std::unique_ptr<ExecutorProcess>> waitingThreads_;
    std::deque<std::unique_ptr<ExecutorProcess>> runningThreads_;
    size_t numEnqueuedThreads_, numFinishedThreads_;

    static const std::string loggerCat_;
};

}   //namespace

#endif
