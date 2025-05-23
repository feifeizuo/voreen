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

#ifndef VRN_OCTREEWALKER_H
#define VRN_OCTREEWALKER_H

#include "voreen/core/processors/asynccomputeprocessor.h"
#include "voreen/core/ports/geometryport.h"
#include "voreen/core/properties/optionproperty.h"
#include "voreen/core/properties/intproperty.h"
#include "voreen/core/properties/floatproperty.h"
#include "voreen/core/properties/boolproperty.h"
#include "voreen/core/properties/optionproperty.h"
#include "voreen/core/properties/temppathproperty.h"
#include "voreen/core/datastructures/octree/volumeoctree.h"

#include "voreen/core/datastructures/geometry/pointsegmentlistgeometry.h"
#include "voreen/core/datastructures/volume/volumeatomic.h"

#include "voreen/core/utils/voreenblas/voreenblascpu.h"

#ifdef VRN_MODULE_OPENMP
#include "modules/openmp/include/voreenblasmp.h"
#endif
#ifdef VRN_MODULE_OPENCL
#include "modules/opencl/utils/voreenblascl.h"
#endif

#include "../util/noisemodel.h"
#include "../util/memprofiler.h"

#include <string>
#include <chrono>
#include <unordered_set>

namespace voreen {

class Volume;
class RandomWalkerSolver;
class RandomWalkerSeeds;
class RandomWalkerWeights;
class OctreeBrickPoolManagerMmap;

struct OctreeWalkerPreviousResult {
public:
    VolumeOctree& octree();
    VolumeBase& volume();

    OctreeWalkerPreviousResult(OctreeWalkerPreviousResult&& other);
    OctreeWalkerPreviousResult& operator=(OctreeWalkerPreviousResult&& other);
    OctreeWalkerPreviousResult(VolumeOctree& octree, std::unique_ptr<VolumeBase>&& volume, PointSegmentListGeometryVec3 foregroundSeeds, PointSegmentListGeometryVec3 backgroundSeeds);
    ~OctreeWalkerPreviousResult();

    void destroyButRetainNodes(std::unordered_set<const VolumeOctreeNode*>& nodesToSave) &&;
    bool isPresent() const;

    PointSegmentListGeometryVec3 foregroundSeeds_;
    PointSegmentListGeometryVec3 backgroundSeeds_;
private:
    VolumeOctree* octree_;                  // NEVER owns its own brickpool manager, ALWAYS a representation of previousVolume
    std::unique_ptr<VolumeBase> volume_;    // ALWAYS stores reference to representation in previousOctree_, never null
};

struct VarianceTree {
    std::unique_ptr<OctreeBrickPoolManagerMmap> brickPoolManager_;
    LocatedVolumeOctreeNode root_;

    static VarianceTree none();
    bool isNone() const;
    VarianceTree(std::unique_ptr<OctreeBrickPoolManagerMmap>&& brickPoolManager, LocatedVolumeOctreeNode root);
    VarianceTree(VarianceTree&& other);
    VarianceTree& operator=(VarianceTree&& other);
    ~VarianceTree();
};


struct OctreeWalkerInput {
    VolumeOctree* previousResult_;
    PointSegmentListGeometryVec3* previousForegroundSeeds_;
    PointSegmentListGeometryVec3* previousBackgroundSeeds_;
    std::unique_ptr<OctreeBrickPoolManagerMmap>& brickPoolManager_;
    const VolumeBase& volume_;
    const VolumeOctree& octree_;
    std::vector<PortDataPointer<Geometry>> foregroundGeomSeeds_;
    std::vector<PortDataPointer<Geometry>> backgroundGeomSeeds_;
    int minWeight_;
    int parameterEstimationNeighborhoodExtent_;
    const VoreenBlas* blas_;
    VoreenBlas::ConjGradPreconditioner precond_;
    float errorThreshold_;
    int maxIterations_;
    float homogeneityThreshold_;
    float binaryPruningDelta_;
    float incrementalSimilarityThreshold_;
    RWNoiseModel noiseModel_;
    const VarianceTree& varianceTree_;
};

struct OctreeWalkerOutput {
    OctreeWalkerOutput(
        OctreeWalkerPreviousResult&& result,
        std::unordered_set<const VolumeOctreeNode*>&& sharedNodes,
        VarianceTree&& varianceTree,
        std::chrono::duration<float> duration
    );
    ~OctreeWalkerOutput();
    OctreeWalkerOutput(OctreeWalkerOutput&& other);

    OctreeWalkerPreviousResult result_;
    std::unordered_set<const VolumeOctreeNode*> sharedNodes_;
    std::chrono::duration<float> duration_;
    VarianceTree varianceTree_;
    bool movedOut_;
};

class OctreeWalker : public AsyncComputeProcessor<OctreeWalkerInput, OctreeWalkerOutput> {
public:
    OctreeWalker();
    virtual ~OctreeWalker();
    virtual Processor* create() const;

    virtual std::string getCategory() const             { return "Volume Processing"; }
    virtual std::string getClassName() const            { return "OctreeWalker";      }
    virtual Processor::CodeState getCodeState() const   { return CODE_STATE_EXPERIMENTAL;  }

    virtual bool isReady() const;

    static const std::string loggerCat_; ///< category used in logging

protected:
    virtual void setDescriptions() {
        setDescription("Performs a semi-automatic octree volume segmentation using a hierarchical 3D random walker algorithm."
                "<br><br>"
                "This processor implements the algorithm described in the article <b>Hierarchical Random Walker Segmentation for Large Volumetric Biomedical Data</b>, Dominik Drees, Florian Eilers and Xiaoyi Jiang (2022), which you are kindly asked to cite if you find it useful in your research."
                "<br><br>"
                "For details on the automatic weight functions refer to the following publications:<br><br>"
                "<b>Statistical Modeling Based Adaptive Parameter Setting for Random Walk Segmentation</b>, Ang Bian and Xioayi Jiang (2016)<br><br>"
                "<b>T-Test Based Adaptive Random Walk Segmentation Under Multiplicative Speckle Noise Model</b>, Ang Bian and Xioayi Jiang (2016)<br><br>"
                "<b>A Bhattacharyya Coefficient-Based Framework for Noise Model-Aware Random Walker Image Segmentation</b>, Dominik Drees, Florian Eilers, Ang Bian and Xioayi Jiang (2022)<br><br>"
                );
    }

    virtual ComputeInput prepareComputeInput();
    virtual ComputeOutput compute(ComputeInput input, ProgressReporter& progressReporter) const;
    virtual void processComputeOutput(ComputeOutput output);

    virtual void serialize(Serializer& s) const;
    virtual void deserialize(Deserializer& s);

    virtual void initialize();
    virtual void deinitialize();

    void clearPreviousResults();

private:
    const VoreenBlas* getVoreenBlasFromProperties() const;

    VolumePort inportVolume_;
    GeometryPort inportForegroundSeeds_;
    GeometryPort inportBackgroundSeeds_;
    VolumePort outportProbabilities_;

    OptionProperty<RWNoiseModel> noiseModel_;
    IntProperty minEdgeWeight_;
    IntProperty parameterEstimationNeighborhoodExtent_;
    StringOptionProperty preconditioner_;
    IntProperty errorThreshold_;
    IntProperty maxIterations_;
    StringOptionProperty conjGradImplementation_;
    FloatProperty homogeneityThreshold_;
    FloatProperty binaryPruningDelta_;
    FloatProperty incrementalSimilarityThreshold_;

    VoreenBlasCPU voreenBlasCPU_;
#ifdef VRN_MODULE_OPENMP
    VoreenBlasMP voreenBlasMP_;
#endif
#ifdef VRN_MODULE_OPENCL
    VoreenBlasCL voreenBlasCL_;
#endif

    ProfileDataCollector ramProfiler_;
    ProfileDataCollector vramProfiler_;

    ButtonProperty clearResult_;
    TempPathProperty resultPath_;
    std::string prevResultPath_;

    boost::optional<OctreeWalkerPreviousResult> previousResult_;
    std::unique_ptr<OctreeBrickPoolManagerMmap> brickPoolManager_;
    VarianceTree varianceTree_;

    // Clock and duration used for time keeping
    typedef std::chrono::steady_clock clock;
};

} //namespace

#endif
