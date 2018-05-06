/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2016 University of Muenster, Germany.                        *
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

#include "vesselgraphcomparison.h"

#include "voreen/core/datastructures/callback/lambdacallback.h"

#include "custommodules/bigdataimageprocessing/util/csvwriter.h"
#include "modules/plotting/datastructures/plotdata.h"
#include "modules/plotting/datastructures/plotrow.h"
#include "modules/plotting/datastructures/plotcell.h"

#include "lemon/concepts/graph.h"
#include "lemon/matching.h"
#include "lemon/smart_graph.h"

#include "tgt/filesystem.h"
#include "tgt/immediatemode/immediatemode.h"


namespace munkres {
#include "../ext/munkres-cpp/src/munkres.h"
}

#include <unordered_map>
#include <array>
#include <numeric>

namespace voreen {

template<class T>
float Matching<T>::someMeasure() const {
    float num_matches = matches_.size();
    float num_not_matched1 = non_matched1_.size();
    float num_not_matched2 = non_matched2_.size();
    float measure = 2*num_matches/(2*num_matches + num_not_matched1 + num_not_matched2);
    tgtAssert(0.0f <= measure && measure <= 1.0f, "Invalid measure");
    return measure;
};


const std::string VesselGraphComparison::loggerCat_("voreen.vesselgraphrenderer");

VesselGraphComparison::VesselGraphComparison()
    : GeometryRendererBase()
    , inport1_(Port::INPORT, "graph.input1", "Graph Input One", false, Processor::INVALID_RESULT)
    , inport2_(Port::INPORT, "graph.input2", "Graph Input Two", false, Processor::INVALID_RESULT)
    //, plotOutport_(Port::OUTPORT, "plot.output", "Plot Data Output", false,  Processor::VALID)
    , enabled_("enabled", "Enabled", true)
    , matchingAlgorithm_("matchingAlgorithm", "Matching Algorithm")
    , statExportFile_("statExportFile", "Stat Export File", "Export File Path", "", "*.csv", FileDialogProperty::SAVE_FILE)
    , datasetIdentifier_("datasetIdentifier", "Dataset identifier", "")
    , nodeMatchRatio_("nodeMatchRatio", "Node Match Ratio", 0.0f, 0.0f, 1.0f)
    , edgeMatchRatio_("edgeMatchRatio", "Edge Match Ratio", 0.0f, 0.0f, 1.0f)
    , lengthSimilarity_("lengthSimilarity", "Length Similarity", 0.0f, 0.0f, 1.0f)
    , crossRadius_("crossRadius", "Cross Radius", 0.0f, 0.0f, 1.0f)
    , lastEdgeMatching_(nullptr)
{

    addPort(inport1_);
    addPort(inport2_);
    //addPort(plotOutport_);

    addProperty(enabled_);
    addProperty(matchingAlgorithm_);
        matchingAlgorithm_.addOption("mutualNN", "Mutual NN", MUTUAL_NN);
        matchingAlgorithm_.addOption("hungarianEdges", "Hungarian on Edges", HUNGARIAN_EDGES);
        matchingAlgorithm_.addOption("hungarianEdgesQuantilThreshold", "Hungarian on Edges with Quantil Threshold", HUNGARIAN_EDGES_QUANTIL_THRESHOLD);
        matchingAlgorithm_.addOption("lap", "Fast Matching with Quantil Threshold", LAP);
        matchingAlgorithm_.selectByValue(LAP);
    addProperty(statExportFile_);
    addProperty(datasetIdentifier_);
    addProperty(nodeMatchRatio_);
        nodeMatchRatio_.setReadOnlyFlag(true);
    addProperty(edgeMatchRatio_);
        edgeMatchRatio_.setReadOnlyFlag(true);
    addProperty(lengthSimilarity_);
        lengthSimilarity_.setReadOnlyFlag(true);

    addProperty(crossRadius_);

}

VesselGraphComparison::~VesselGraphComparison() {
}

void VesselGraphComparison::process() {
    lastEdgeMatching_.reset(nullptr);
    if(!enabled_.get()) {
        return;
    }
    const VesselGraph* input1 = inport1_.getData();
    const VesselGraph* input2 = inport2_.getData();
    if(!input1 || !input2) {
        LWARNING("No full graph input");
        return;
    }
    compare(*input1, *input2);
}

bool VesselGraphComparison::isReady() const {
    return isInitialized() && inport1_.isReady() && inport2_.isReady();
}

// ----------------------------------------------------------
// Property Comparison
// ----------------------------------------------------------

// Property accessors

struct LengthProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getLength();
    }
};
struct DistanceProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getDistance();
    }
};
struct StraightnessProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getStraightness();
    }
};
struct VolumeProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getVolume();
    }
};
struct AvgCrossSectionProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getAvgCrossSection();
    }
};
struct MinRadiusMeanProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getMinRadiusAvg();
    }
};
struct MinRadiusStdProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getMinRadiusStdDeviation();
    }
};
struct AvgRadiusMeanProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getAvgRadiusAvg();
    }
};
struct AvgRadiusStdProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getAvgRadiusStdDeviation();
    }
};
struct MaxRadiusMeanProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getMaxRadiusAvg();
    }
};
struct MaxRadiusStdProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getMaxRadiusStdDeviation();
    }
};
struct RoundnessMeanProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getRoundnessAvg();
    }
};
struct RoundnessStdProperty {
    static float getProperty(const VesselGraphEdge& e) {
        return e.getRoundnessStdDeviation();
    }
};

// ----------------------------------------------------------
// Property Comparators
// ----------------------------------------------------------

struct AbsoluteError {
    template<class T>
    static float compare(const VesselGraphEdge& e1, const VesselGraphEdge& e2) {
        float p1 = T::getProperty(e1);
        float p2 = T::getProperty(e2);
        tgtAssert(p1 >= 0, "Negative property");
        tgtAssert(p2 >= 0, "Negative property");
        return std::abs(p1 - p2);
    }
};

struct RelativeError {
    template<class T>
    static float compare(const VesselGraphEdge& e1, const VesselGraphEdge& e2) {
        float max = std::max(T::getProperty(e1), T::getProperty(e2));
        float abs_err = AbsoluteError::compare<T>(e1, e2);
        if(max == 0) {
            tgtAssert(abs_err == 0, "Invalid relative error (negative attributes?)");
            return 0;
        }
        float err = abs_err/max;
        tgtAssert(0 <= err && err <= 1, "Invalid relative error");
        return err;
    }
};

struct ExpSimilarity {
    template<class T>
    static float compare(const VesselGraphEdge& e1, const VesselGraphEdge& e2) {
        return std::exp(-AbsoluteError::compare<T>(e1, e2));
    }
};

template<class T, class S>
float compareMatches(const std::vector<std::pair<const VesselGraphEdge*, const VesselGraphEdge*>>& matches) {
    float sum = 0;
    for(const auto& pair : matches) {
        tgtAssert(pair.first, "nullptr match");
        tgtAssert(pair.second, "nullptr match");
        const VesselGraphEdge& e1 = *pair.first;
        const VesselGraphEdge& e2 = *pair.second;
        bool e1valid = e1.hasValidData();
        bool e2valid = e2.hasValidData();
        if(e1valid && e2valid) {
            sum += S::template compare<T>(e1, e2);
        } else if(!e1valid && !e2valid) {
            sum += 0;
        } else {
            tgtAssert(false, "Matched valid and invalid edge");
        }
    }
    return sum/matches.size();
}

// ----------------------------------------------------------
// Edge Distances
// ----------------------------------------------------------
const float INVALID_MATCH_DISTANCE = std::numeric_limits<float>::max();

struct SimpleEdgeDistance {
    float distance(const VesselGraphEdge& e1, const VesselGraphEdge& e2, float known_max_distance = 0) const {
        float d1sq = tgt::distanceSq(e1.getNode1().pos_, e2.getNode1().pos_) + tgt::distanceSq(e1.getNode2().pos_, e2.getNode2().pos_);
        float d2sq = tgt::distanceSq(e1.getNode1().pos_, e2.getNode2().pos_) + tgt::distanceSq(e1.getNode2().pos_, e2.getNode1().pos_);
        float node_distance = std::sqrt(std::min(d1sq, d2sq));
        float node_distance_plus_eps = node_distance + std::numeric_limits<float>::epsilon()*known_max_distance;

        bool e1valid = e1.hasValidData();
        bool e2valid = e2.hasValidData();
        if(!e1valid || !e2valid) {
            if(e1valid == e2valid) {
                //Both do not have valid data => only match them by their spatial positions
                return node_distance_plus_eps;
            } else {
                return INVALID_MATCH_DISTANCE;
            }
        }

        const std::array<float, 13> properties = {
           RelativeError::compare<LengthProperty>(e1, e2),
           RelativeError::compare<DistanceProperty>(e1, e2),
           RelativeError::compare<StraightnessProperty>(e1, e2),
           RelativeError::compare<VolumeProperty>(e1, e2),
           RelativeError::compare<AvgCrossSectionProperty>(e1, e2),
           RelativeError::compare<MinRadiusMeanProperty>(e1, e2),
           RelativeError::compare<MinRadiusStdProperty>(e1, e2),
           RelativeError::compare<AvgRadiusMeanProperty>(e1, e2),
           RelativeError::compare<AvgRadiusStdProperty>(e1, e2),
           RelativeError::compare<MaxRadiusMeanProperty>(e1, e2),
           RelativeError::compare<MaxRadiusStdProperty>(e1, e2),
           RelativeError::compare<RoundnessMeanProperty>(e1, e2),
           RelativeError::compare<RoundnessStdProperty>(e1, e2)
        };


        const float num_properties = properties.size();
        float property_sum = std::accumulate(properties.begin(), properties.end(), 0.0f);
        float relative_property_error = property_sum/num_properties;
        tgtAssert(0 <= relative_property_error && relative_property_error < 1, "invalid relative property error");
        return node_distance_plus_eps / (1.0f - relative_property_error);
    }
};

template<class D>
struct ThresholdedEdgeDistance {
    ThresholdedEdgeDistance(float threshold, D distanceFunc)
        : threshold_(threshold)
        , distanceFunc_(distanceFunc)
    {
    }

    float distance(const VesselGraphEdge& e1, const VesselGraphEdge& e2) const {
        float dist = distanceFunc_.distance(e1, e2, threshold_);
        if(dist <= threshold_) {
            return dist;
        } else {
            return INVALID_MATCH_DISTANCE;
        }
    }

    const float threshold_;
    const D distanceFunc_;
};

// ----------------------------------------------------------
// VesselGraphComparison Implementation
// ----------------------------------------------------------

const VesselGraphNode* findClosest(const VesselGraphNode& n, const std::vector<VesselGraphNode>& candidates) {
    const VesselGraphNode* closest = nullptr;
    float closest_dist_sq = std::numeric_limits<float>::max();
    for(const auto& candidate : candidates) {
        float dist_sq = tgt::distanceSq(candidate.pos_, n.pos_);
        if(dist_sq < closest_dist_sq) {
            closest_dist_sq = dist_sq;
            closest = &candidate;
        }
    }
    return closest;
}

Matching<VesselGraphNode> VesselGraphComparison::matchNodesMutualNN(const VesselGraph& g1, const VesselGraph& g2) const {
    Matching<VesselGraphNode> output;
    const std::vector<VesselGraphNode>& n1 = g1.getNodes();
    const std::vector<VesselGraphNode>& n2 = g2.getNodes();
    for(const auto& node : n1) {
        const VesselGraphNode* closest_in_2 = findClosest(node, n2);
        tgtAssert(closest_in_2, "No closest node");

        const VesselGraphNode* closest_in_1 = findClosest(*closest_in_2, n1);
        if(&node == closest_in_1) {
            output.matches_.push_back({&node, closest_in_2});
        } else {
            output.non_matched1_.push_back(&node);
        }
    }
    for(const auto& node : n2) {
        bool found = false;
        for(const auto& matching : output.matches_) {
            if(&node == matching.second) {
                found = true;
                break;
            }
        }
        if(!found) {
            output.non_matched2_.push_back(&node);
        }
    }

    tgtAssert(output.matches_.size() + output.non_matched1_.size() == n1.size(), "Node Matching failed");
    tgtAssert(output.matches_.size() + output.non_matched2_.size() == n2.size(), "Node Matching failed");

    return output;
}

const VesselGraphEdge* findBestEdgeMatch(
        const VesselGraphEdge& edge,
        const std::unordered_map<const VesselGraphNode*, const VesselGraphNode*>& matched_nodes_1_to_2
        ) {
    const VesselGraphNode& n1 = edge.getNode1();
    const VesselGraphNode& n2 = edge.getNode2();

    const auto maybe_n1_in_2 = matched_nodes_1_to_2.find(&n1);
    if(maybe_n1_in_2 == matched_nodes_1_to_2.cend()) {
        return nullptr;
    }
    const VesselGraphNode* n1_in_2 = maybe_n1_in_2->second;


    const auto maybe_n2_in_2 = matched_nodes_1_to_2.find(&n2);
    if(maybe_n2_in_2 == matched_nodes_1_to_2.cend()) {
        return nullptr;
    }
    const VesselGraphNode* n2_in_2 = maybe_n2_in_2->second;

    std::vector<const VesselGraphEdge*> edge_candidates;
    for(const auto& edge : n1_in_2->getEdges()) {
        if(edge.get().getNodeID1() == n2_in_2->getID() || edge.get().getNodeID2() == n2_in_2->getID()) {
            edge_candidates.push_back(&edge.get());
        }
    }

    const VesselGraphEdge* best_match = nullptr;
    float smallest_edge_prop_diff = std::numeric_limits<float>::max();
    for(const auto& candidate : edge_candidates) {
        float edge_prop_diff = std::abs(candidate->getLength() - edge.getLength());
        if(edge_prop_diff < smallest_edge_prop_diff) {
            smallest_edge_prop_diff = edge_prop_diff;
            best_match = candidate;
        }
    }
    return best_match;
}

Matching<VesselGraphEdge> VesselGraphComparison::matchEdgesViaNodes(const VesselGraph& g1, const VesselGraph& g2, const Matching<VesselGraphNode>& node_matching) const {
    Matching<VesselGraphEdge> output;
    const std::vector<VesselGraphEdge>& e1 = g1.getEdges();
    const std::vector<VesselGraphEdge>& e2 = g2.getEdges();

    std::unordered_map<const VesselGraphNode*, const VesselGraphNode*> matched_nodes_1_to_2;
    std::unordered_map<const VesselGraphNode*, const VesselGraphNode*> matched_nodes_2_to_1;
    for(const auto& match : node_matching.matches_) {
        matched_nodes_1_to_2.insert(match);
        matched_nodes_2_to_1.insert({match.second, match.first});
    }

    for(const auto& edge : e1) {
        const VesselGraphEdge* match = findBestEdgeMatch(edge, matched_nodes_1_to_2);
        if(match) {
            //TODO: do or do not do reverse check?
            const VesselGraphEdge* reverse_match = findBestEdgeMatch(*match, matched_nodes_2_to_1);
            if(reverse_match == &edge) {
                output.matches_.push_back({&edge, match});
            } else {
                output.non_matched1_.push_back(&edge);
            }
        } else {
            output.non_matched1_.push_back(&edge);
        }
    }
    for(const auto& edge : e2) {
        bool found = false;
        for(const auto& matching : output.matches_) {
            if(&edge == matching.second) {
                found = true;
                break;
            }
        }
        if(!found) {
            output.non_matched2_.push_back(&edge);
        }
    }

    tgtAssert(output.matches_.size() + output.non_matched1_.size() == e1.size(), "Edge Matching failed");
    tgtAssert(output.matches_.size() + output.non_matched2_.size() == e2.size(), "Edge Matching failed");

    return output;
}

// Munkres version (slow)
template<class D>
Matching<VesselGraphEdge> VesselGraphComparison::matchEdgesViaHungarianAlgorithm(const VesselGraph& g1, const VesselGraph& g2, D distance) const {
    const std::vector<VesselGraphEdge>& e1 = g1.getEdges();
    const std::vector<VesselGraphEdge>& e2 = g2.getEdges();
    munkres::Matrix<float> dist_mat(e1.size(), e2.size());

    {
        size_t index1 = 0;
        for(const auto& edge1 : e1) {
            size_t index2 = 0;
            for(const auto& edge2 : e2) {
                dist_mat(index1, index2) = distance.distance(edge1, edge2);
                ++index2;
            }
            ++index1;
        }
    }
    munkres::Matrix<float> original_dist_mat(dist_mat);

    munkres::Munkres<float> solver;
    solver.solve(dist_mat);

    Matching<VesselGraphEdge> output;

    {
        size_t index1 = 0;
        for(const auto& edge1 : e1) {
            size_t index2 = 0;
            for(const auto& edge2 : e2) {
                if(dist_mat(index1, index2) == 0 && original_dist_mat(index1, index2) != INVALID_MATCH_DISTANCE) {
                    output.matches_.push_back({&edge1, &edge2});
                    break;
                }
                ++index2;
                if(index2 == e2.size()) {
                    output.non_matched1_.push_back(&edge1);
                }
            }
            ++index1;
        }
    }

    for(const auto& edge : e2) {
        bool found = false;
        for(const auto& matching : output.matches_) {
            if(&edge == matching.second) {
                found = true;
                break;
            }
        }
        if(!found) {
            output.non_matched2_.push_back(&edge);
        }
    }

    tgtAssert(output.matches_.size() + output.non_matched1_.size() == e1.size(), "Edge Matching failed");
    tgtAssert(output.matches_.size() + output.non_matched2_.size() == e2.size(), "Edge Matching failed");

    return output;
}


// Lemon version (faster, hopefully)
template<class D>
Matching<VesselGraphEdge> VesselGraphComparison::matchEdgesLAP(const VesselGraph& g1, const VesselGraph& g2, D distance) const {
    const std::vector<VesselGraphEdge>& e1 = g1.getEdges();
    const std::vector<VesselGraphEdge>& e2 = g2.getEdges();
    munkres::Matrix<float> dist_mat(e1.size(), e2.size());

    typedef lemon::SmartGraph Graph;
    typedef Graph::Node Node;
    //typedef Graph::Edge Edge;
    typedef Graph::EdgeMap<float> WeightMap;

    Graph g;
    WeightMap w(g);

    std::unordered_map<const VesselGraphEdge*, Node> vessel_to_lemon;
    std::map<Node, const VesselGraphEdge*> lemon_to_vessel;
    for(const auto& edge : e1) {
        auto node = g.addNode();
        vessel_to_lemon.insert(std::make_pair(&edge, node));
        lemon_to_vessel.insert(std::make_pair(node, &edge));
    }
    for(const auto& edge : e2) {
        auto node = g.addNode();
        vessel_to_lemon.insert(std::make_pair(&edge, node));
        lemon_to_vessel.insert(std::make_pair(node, &edge));
    }

    for(const auto& edge1 : e1) {
        for(const auto& edge2 : e2) {
            float d = distance.distance(edge1, edge2);
            if(d != INVALID_MATCH_DISTANCE) {
                auto edge = g.addEdge(vessel_to_lemon[&edge1], vessel_to_lemon[&edge2]);
                w.set(edge, distance.threshold_ - d);
            }
        }
    }


    lemon::MaxWeightedMatching<Graph, WeightMap> matching(g, w);
    matching.run();

    Matching<VesselGraphEdge> output;

    {
        for(const auto& edge : e1) {
            Node mate = matching.mate(vessel_to_lemon[&edge]);
            if(mate != lemon::INVALID) {
                output.matches_.push_back({&edge, lemon_to_vessel[mate]});
            } else {
                output.non_matched1_.push_back(&edge);
            }
        }
    }

    for(const auto& edge : e2) {
        bool found = false;
        for(const auto& matching : output.matches_) {
            if(&edge == matching.second) {
                found = true;
                break;
            }
        }
        if(!found) {
            output.non_matched2_.push_back(&edge);
        }
    }

    tgtAssert(output.matches_.size() + output.non_matched1_.size() == e1.size(), "Edge Matching failed");
    tgtAssert(output.matches_.size() + output.non_matched2_.size() == e2.size(), "Edge Matching failed");

    return output;
}


/*
template<class T, class S>
void putPlotMatchingData(PlotPort& port, const VesselGraph& g1, const VesselGraph& g2) {
    const std::vector<VesselGraphEdge>& e1 = g1.getEdges();
    const std::vector<VesselGraphEdge>& e2 = g2.getEdges();
    std::unique_ptr<PlotData> plotData(new PlotData(0, 2));
    plotData->setColumnLabel(0, "index");
    plotData->setColumnLabel(1, "Data");
    size_t index = 0;
    for(const auto& edge1 : e1) {
        for(const auto& edge2 : e2) {
            std::vector<PlotCellValue> v(2);
            v.at(0) = PlotCellValue(static_cast<plot_t>(++index));
            v.at(1) = edgeDistance(edge1, edge2);
            tgtAssert(plotData->insert(v), "insert failed");
        }
    }
    port.setData(plotData.release());
}
*/

template<class D>
float quantilThreshold(const VesselGraph& g1, const VesselGraph& g2, D distanceFunc, size_t threshold_index) {
    const std::vector<VesselGraphEdge>& e1 = g1.getEdges();
    const std::vector<VesselGraphEdge>& e2 = g2.getEdges();

    // This is a quick and dirty implementation with room for (performance) improvements.
    std::vector<float> distances;
    distances.reserve(e1.size()*e2.size());
    for(const auto& edge1 : e1) {
        for(const auto& edge2 : e2) {
            distances.push_back(distanceFunc.distance(edge1, edge2));
        }
    }

    std::sort(distances.begin(), distances.end());

    tgtAssert(threshold_index < distances.size(), "Invalid threshold_index");
    return distances[threshold_index];
}

void VesselGraphComparison::compare(const VesselGraph& g1, const VesselGraph& g2) {
    // 1. step: match nodes
    Matching<VesselGraphNode> node_matching = matchNodesMutualNN(g1, g2);
    nodeMatchRatio_.set(node_matching.someMeasure());

    // 2. step: match edges? difficulties: we have multigraphs!
    Matching<VesselGraphEdge> edge_matching;
    switch(matchingAlgorithm_.getValue()) {
        case MUTUAL_NN:
            edge_matching = matchEdgesViaNodes(g1, g2, node_matching);
            break;
        case HUNGARIAN_EDGES:
            edge_matching = matchEdgesViaHungarianAlgorithm(g1, g2, SimpleEdgeDistance());
            break;
        case HUNGARIAN_EDGES_QUANTIL_THRESHOLD:
            {
                float threshold = quantilThreshold(g1, g2, SimpleEdgeDistance(), 2*std::min(g1.getEdges().size(), g2.getEdges().size()));
                edge_matching = matchEdgesViaHungarianAlgorithm(g1, g2, ThresholdedEdgeDistance<SimpleEdgeDistance>(threshold, SimpleEdgeDistance()));
            }
            break;
        case LAP:
            {
                float threshold = quantilThreshold(g1, g2, SimpleEdgeDistance(), 2*std::min(g1.getEdges().size(), g2.getEdges().size()));
                edge_matching = matchEdgesLAP(g1, g2, ThresholdedEdgeDistance<SimpleEdgeDistance>(threshold, SimpleEdgeDistance()));
            }
            break;
        default:
            tgtAssert(false, "Invalid matching algorithm");
    }
    edgeMatchRatio_.set(edge_matching.someMeasure());

    // 3. step: Compute measure based on properties of matches
    lengthSimilarity_.set(edge_matching.someMeasure()*(1.0f-compareMatches<LengthProperty, RelativeError>(edge_matching.matches_)));

    const std::string statExportFileName = statExportFile_.get();

    if(statExportFileName.empty()) {
        LWARNING("No Stat export file path. Skipping writing stats to file.");
    } else {
        try {
            bool truncate = !tgt::FileSystem::fileExists(statExportFileName);
            CSVWriteMode writeMode = truncate ? CSVWriteMode::TRUNCATE : CSVWriteMode::APPEND;
            CSVWriter<std::string, float, float
                , float, float, float
                , float, float, float
                , float, float, float
                , float, float, float
                , float, float, float
                , float, float, float
                , float, float, float
                , float, float, float
                , float, float, float
                , float, float, float
                , float, float, float
                , float, float, float
                , float, float, float
                    > writer(statExportFileName, ';', writeMode);
            if(truncate) {
                writer.writeHeader("identifier", "node match ratio", "edge match ratio"
                    , "length abs error", "length rel error", "length exp similarity"
                    , "distance abs error", "distance rel error", "distance exp similarity"
                    , "straightness abs error", "straightness rel error", "straightness exp similarity"
                    , "volume abs error", "volume rel error", "volume exp similarity"
                    , "avgCrossSection abs error", "avgCrossSection rel error", "avgCrossSection exp similarity"
                    , "minRadiusMean abs error", "minRadiusMean rel error", "minRadiusMean exp similarity"
                    , "minRadiusStd abs error", "minRadiusStd rel error", "minRadiusStd exp similarity"
                    , "avgRadiusMean abs error", "avgRadiusMean rel error", "avgRadiusMean exp similarity"
                    , "avgRadiusStd abs error", "avgRadiusStd rel error", "avgRadiusStd exp similarity"
                    , "maxRadiusMean abs error", "maxRadiusMean rel error", "maxRadiusMean exp similarity"
                    , "maxRadiusStd abs error", "maxRadiusStd rel error", "maxRadiusStd exp similarity"
                    , "roundnessMean abs error", "roundnessMean rel error", "roundnessMean exp similarity"
                    , "roundnessStd abs error", "roundnessStd rel error", "roundnessStd exp similarity"
                    );
            }
            writer.write(
                      datasetIdentifier_.get()
                    , node_matching.someMeasure()
                    , edge_matching.someMeasure()
                    , compareMatches<LengthProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<LengthProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<LengthProperty, ExpSimilarity>(edge_matching.matches_)
                    , compareMatches<DistanceProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<DistanceProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<DistanceProperty, ExpSimilarity>(edge_matching.matches_)
                    , compareMatches<StraightnessProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<StraightnessProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<StraightnessProperty, ExpSimilarity>(edge_matching.matches_)
                    , compareMatches<VolumeProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<VolumeProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<VolumeProperty, ExpSimilarity>(edge_matching.matches_)
                    , compareMatches<AvgCrossSectionProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<AvgCrossSectionProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<AvgCrossSectionProperty, ExpSimilarity>(edge_matching.matches_)
                    , compareMatches<MinRadiusMeanProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<MinRadiusMeanProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<MinRadiusMeanProperty, ExpSimilarity>(edge_matching.matches_)
                    , compareMatches<MinRadiusStdProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<MinRadiusStdProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<MinRadiusStdProperty, ExpSimilarity>(edge_matching.matches_)
                    , compareMatches<AvgRadiusMeanProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<AvgRadiusMeanProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<AvgRadiusMeanProperty, ExpSimilarity>(edge_matching.matches_)
                    , compareMatches<AvgRadiusStdProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<AvgRadiusStdProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<AvgRadiusStdProperty, ExpSimilarity>(edge_matching.matches_)
                    , compareMatches<MaxRadiusMeanProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<MaxRadiusMeanProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<MaxRadiusMeanProperty, ExpSimilarity>(edge_matching.matches_)
                    , compareMatches<MaxRadiusStdProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<MaxRadiusStdProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<MaxRadiusStdProperty, ExpSimilarity>(edge_matching.matches_)
                    , compareMatches<RoundnessMeanProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<RoundnessMeanProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<RoundnessMeanProperty, ExpSimilarity>(edge_matching.matches_)
                    , compareMatches<RoundnessStdProperty, AbsoluteError>(edge_matching.matches_)
                    , compareMatches<RoundnessStdProperty, RelativeError>(edge_matching.matches_)
                    , compareMatches<RoundnessStdProperty, ExpSimilarity>(edge_matching.matches_)
                        );
            LINFO("Writing edge stats: " << statExportFileName);
        } catch(tgt::IOException& e) {
            LERROR("Error opening file " << statExportFileName << ": " << e.what());
        }
    }

    lastEdgeMatching_.reset(new Matching<VesselGraphEdge>(std::move(edge_matching)));

    //putPlotMatchingData<LengthProperty, RelativeError>(plotOutport_, g1, g2);
}

void renderEdgeCenters(const std::vector<const VesselGraphEdge*> edges, float r1, float r2) {
    for(auto edge : edges) {
        tgtAssert(edge, "no edge");
        tgt::vec3 center = (edge->getNode1().pos_ + edge->getNode2().pos_)*0.5f;

        for(int dim = 0; dim < 3; ++dim) {
            tgt::vec3 pos1 = center;
            pos1[dim] += r1;
            tgt::vec3 pos2 = center;
            pos2[dim] -= r2;

            IMode.vertex(pos1);
            IMode.vertex(pos2);
        }

    }
}

void renderMatches(const std::vector<std::pair<const VesselGraphEdge*, const VesselGraphEdge*>> matches) {
    for(auto match : matches) {
        const VesselGraphEdge* edge1 = match.first;
        const VesselGraphEdge* edge2 = match.second;
        tgtAssert(edge1, "no edge");
        tgtAssert(edge2, "no edge");
        tgt::vec3 center1 = (edge1->getNode1().pos_ + edge1->getNode2().pos_)*0.5f;
        tgt::vec3 center2 = (edge2->getNode1().pos_ + edge2->getNode2().pos_)*0.5f;

        IMode.color(tgt::vec4(1,0,0,1));
        IMode.vertex(center1);
        IMode.color(tgt::vec4(0,0,1,1));
        IMode.vertex(center2);
    }
}

void VesselGraphComparison::render() {
    if(!lastEdgeMatching_) {
        return;
    }
    float radiusbase = crossRadius_.get();
    glLineWidth(3.0f);
    const Matching<VesselGraphEdge>& matching = *lastEdgeMatching_;
    IMode.begin(tgt::ImmediateMode::LINES);
    IMode.color(tgt::vec4(0.5f,0,0,1));
    renderEdgeCenters(matching.non_matched1_, radiusbase*0.8, radiusbase*1.2);
    IMode.color(tgt::vec4(0,0,0.5f,1));
    renderEdgeCenters(matching.non_matched2_, radiusbase*1.2, radiusbase*0.8);
    renderMatches(matching.matches_);
    IMode.color(tgt::vec4(1));
    IMode.end();
    glLineWidth(1.0f);
}
} // namespace voreen