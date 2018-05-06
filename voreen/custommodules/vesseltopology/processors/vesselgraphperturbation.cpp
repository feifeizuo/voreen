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

#include "vesselgraphperturbation.h"
#include "voreen/core/datastructures/callback/lambdacallback.h"
#include "../algorithm/vesselgraphnormalization.h"

#include <unordered_set>
#include <unordered_map>

namespace voreen {

const std::string VesselGraphPerturbation::loggerCat_("voreen.vesselgraphnormalizer");


typedef std::mt19937 random_engine_type;

VesselGraphPerturbation::VesselGraphPerturbation()
    : Processor()
    , inport_(Port::INPORT, "graph.input", "Graph Input", false, Processor::INVALID_RESULT)
    , outport_(Port::OUTPORT, "graph.output", "Perturbed Graph Output", false, Processor::VALID)
    , enabled_("enabled", "Enabled", true)
    , perturbationMethod_("perturbationMethod", "Perturbation Method")
    , perturbationAmount_("perturbationAmount", "Perturbation Amount", 0.5, 0.0, 1.0)
    , usePredeterminedSeed_("usePredeterminedSeed", "Use Predetermined Seed", false)
    , predeterminedSeed_("predeterminedSeed", "Seed", 0, std::numeric_limits<int>::min(), std::numeric_limits<int>::max())
    , randomDevice()
{

    addPort(inport_);
    addPort(outport_);

    addProperty(enabled_);

    addProperty(perturbationMethod_);
        perturbationMethod_.addOption("add_edges", "Add Edges", ADD_EDGES);
        perturbationMethod_.addOption("split_nodes", "Split Nodes", SPLIT_NODES);
        perturbationMethod_.addOption("subdivide_edges", "Subdivide Edges", SUBDIVIDE_EDGES);
        perturbationMethod_.addOption("split_edges", "Split Edges", SPLIT_EDGES);
        perturbationMethod_.addOption("move_nodes", "Move Nodes", MOVE_NODES);
        perturbationMethod_.addOption("change_properties", "Change Properties", CHANGE_PROPERTIES);
        perturbationMethod_.selectByValue(ADD_EDGES);

    addProperty(perturbationAmount_);

    addProperty(usePredeterminedSeed_);
        ON_CHANGE_LAMBDA(usePredeterminedSeed_, [this] {
                    predeterminedSeed_.setVisibleFlag(usePredeterminedSeed_.get());
                });
    addProperty(predeterminedSeed_);
}

VesselGraphPerturbation::~VesselGraphPerturbation() {
}

typedef std::normal_distribution<float> normal_distribution;
typedef std::lognormal_distribution<float> lognormal_distribution;

static normal_distribution getGraphEdgePropertyDestribution(const VesselGraph& graph, std::function<float(const VesselGraphEdge&)> property) {
    float mean, stddev;
    graph.getEdgePropertyStats(property, mean, stddev);
    return normal_distribution(mean, stddev);
}

float make_positive(float val) {
    return std::max(std::numeric_limits<float>::epsilon(), val);
}

float make_non_negative(float val) {
    return std::max(0.0f, val);
}

float clamp_01(float val) {
    return std::min(1.0f, std::max(0.0f, val));
}

struct EdgeGenerator {
    normal_distribution distr_distance_;
    normal_distribution distr_length_;
    normal_distribution distr_straightness_;
    normal_distribution distr_minRadiusAvg_;
    normal_distribution distr_minRadiusStdDeviation_;
    normal_distribution distr_maxRadiusAvg_;
    normal_distribution distr_maxRadiusStdDeviation_;
    normal_distribution distr_avgRadiusAvg_;
    normal_distribution distr_avgRadiusStdDeviation_;
    normal_distribution distr_roundnessAvg_;
    normal_distribution distr_roundnessStdDeviation_;

    EdgeGenerator(const VesselGraph& templateGraph)
        : distr_distance_(getGraphEdgePropertyDestribution(templateGraph, std::mem_fn(&VesselGraphEdge::getDistance)))
        , distr_length_(getGraphEdgePropertyDestribution(templateGraph, std::mem_fn(&VesselGraphEdge::getLength)))
        , distr_straightness_(getGraphEdgePropertyDestribution(templateGraph, std::mem_fn(&VesselGraphEdge::getStraightness)))
        , distr_minRadiusAvg_(getGraphEdgePropertyDestribution(templateGraph, std::mem_fn(&VesselGraphEdge::getMinRadiusAvg)))
        , distr_minRadiusStdDeviation_(getGraphEdgePropertyDestribution(templateGraph, std::mem_fn(&VesselGraphEdge::getMinRadiusStdDeviation)))
        , distr_maxRadiusAvg_(getGraphEdgePropertyDestribution(templateGraph, std::mem_fn(&VesselGraphEdge::getMaxRadiusAvg)))
        , distr_maxRadiusStdDeviation_(getGraphEdgePropertyDestribution(templateGraph, std::mem_fn(&VesselGraphEdge::getMaxRadiusStdDeviation)))
        , distr_avgRadiusAvg_(getGraphEdgePropertyDestribution(templateGraph, std::mem_fn(&VesselGraphEdge::getAvgRadiusAvg)))
        , distr_avgRadiusStdDeviation_(getGraphEdgePropertyDestribution(templateGraph, std::mem_fn(&VesselGraphEdge::getAvgRadiusStdDeviation)))
        , distr_roundnessAvg_(getGraphEdgePropertyDestribution(templateGraph, std::mem_fn(&VesselGraphEdge::getRoundnessAvg)))
        , distr_roundnessStdDeviation_(getGraphEdgePropertyDestribution(templateGraph, std::mem_fn(&VesselGraphEdge::getRoundnessStdDeviation)))
    {
    }

    VesselGraphEdgePathProperties generateEdgePathProperties(random_engine_type random_engine, float distance) {
        VesselGraphEdgePathProperties output;
        //TODO: We might want to do something more sophisticated here and look at correlations between properties.
        //      Might be some amount of work...
        if(distance > 0) {
            output.length_ = distance/distr_straightness_(random_engine);
        } else {
            output.length_ = make_positive(distr_length_(random_engine)); // Account for loops
        }
        std::vector<float> radii;
        radii.push_back(make_positive(distr_minRadiusAvg_(random_engine)));
        radii.push_back(make_positive(distr_avgRadiusAvg_(random_engine)));
        radii.push_back(make_positive(distr_maxRadiusAvg_(random_engine)));
        std::sort(radii.begin(), radii.end());
        output.minRadiusAvg_ = radii.at(0);
        output.minRadiusStdDeviation_ = make_non_negative(distr_minRadiusStdDeviation_(random_engine));
        output.avgRadiusAvg_ = radii.at(1);
        output.avgRadiusStdDeviation_ = make_non_negative(distr_avgRadiusStdDeviation_(random_engine));
        output.maxRadiusAvg_ = radii.at(2);
        output.maxRadiusStdDeviation_ = make_non_negative(distr_maxRadiusStdDeviation_(random_engine));
        output.roundnessAvg_ = clamp_01(distr_roundnessAvg_(random_engine));
        output.roundnessStdDeviation_ = make_non_negative(distr_roundnessStdDeviation_(random_engine));

        output.volume_ = output.length_*output.avgRadiusAvg_*output.avgRadiusAvg_;
        return output;
    }
};

static VesselGraphEdgePathProperties modifyEdgePathProperties(random_engine_type& random_engine, float amount, const VesselGraphEdgePathProperties& base) {
    VesselGraphEdgePathProperties output;
    // Generate the edge that subdivides the first
    auto dist = lognormal_distribution(0, amount);

    auto transform = [&random_engine, &dist] (float val) {
        if(val == VesselGraphEdgePathProperties::INVALID_DATA) {
            return val;
        } else {
            float new_val = dist(random_engine)*val;
            tgtAssert(new_val >= 0, "Negative property");
            return new_val;
        }
    };

    output.length_ = transform(base.length_);

    output.minRadiusAvg_ = transform(base.minRadiusAvg_);
    output.minRadiusStdDeviation_ = transform(base.minRadiusStdDeviation_);
    output.maxRadiusAvg_ = transform(base.maxRadiusAvg_);
    output.maxRadiusStdDeviation_ = transform(base.maxRadiusStdDeviation_);
    output.avgRadiusAvg_ = transform(base.avgRadiusAvg_);
    output.avgRadiusStdDeviation_ = transform(base.avgRadiusStdDeviation_);
    output.roundnessAvg_ = transform(base.roundnessAvg_);
    output.roundnessStdDeviation_ = transform(base.roundnessStdDeviation_);

    output.volume_ = output.length_*output.avgRadiusAvg_*output.avgRadiusAvg_;

    return output;
}

template<class T>
static std::vector<T> draw_n(std::vector<T>&& input, size_t n, random_engine_type& random_engine) {
    tgtAssert(n <= input.size(), "Invalid number of elements to draw");

    std::vector<T> output;
    for(size_t i=0; i<n; ++i) {
        auto& drawn = input.at(std::uniform_int_distribution<size_t>(0, input.size()-1)(random_engine));
        output.push_back(drawn);
        // Removal via the clausing method:
        std::swap(drawn, input.back());
        input.pop_back();
    }
    return output;
}

static std::vector<const VesselGraphEdge*> draw_edges(const std::vector<VesselGraphEdge>& edges, float percentage, random_engine_type random_engine) {
    std::vector<const VesselGraphEdge*> remainig_edge_refs;
    for(const auto& edge : edges) {
        remainig_edge_refs.push_back(&edge);
    }
    return draw_n(std::move(remainig_edge_refs), std::floor(percentage*edges.size()), random_engine);
}

static std::vector<const VesselGraphNode*> draw_nodes_if(const std::vector<VesselGraphNode>& nodes, float percentage, std::function<bool(const VesselGraphNode&)> viable, random_engine_type random_engine) {

    std::vector <const VesselGraphNode*> viable_nodes;
    for(auto& node: nodes) {
        if(viable(node)) {
            viable_nodes.push_back(&node);
        }
    }
    return draw_n(std::move(viable_nodes), std::floor(percentage*viable_nodes.size()), random_engine);
}

static tgt::vec3 generate_random_direction(random_engine_type& random_engine) {
    std::uniform_real_distribution<float> direction_distr(-1, 1);
    tgt::vec3 new_direction;
    do {
        new_direction = tgt::vec3(
                direction_distr(random_engine),
                direction_distr(random_engine),
                direction_distr(random_engine)
                );
    } while (tgt::lengthSq(new_direction) > 1 || new_direction == tgt::vec3::zero);
    return tgt::normalize(new_direction);
}


static std::unique_ptr<VesselGraph> addEdges(const VesselGraph& input, float amount, random_engine_type random_engine) {
    EdgeGenerator edgeGenerator(input);
    std::unique_ptr<VesselGraph> output(new VesselGraph(input));

    //Find pairs of nodes that are both connected to one other node:
    std::vector<std::pair<const VesselGraphNode*, const VesselGraphNode*>> pairs;
    for(auto& node : output->getNodes()) {
        for(auto& b1: node.getEdges()) {
            for(auto& b2: node.getEdges()) {
                if(&b1 == &b2 || b1.get().isLoop() || b2.get().isLoop()) {
                    continue;
                }
                const VesselGraphNode& node1 = b1.get().getOtherNode(node);
                const VesselGraphNode& node2 = b2.get().getOtherNode(node);
                if(&node1 == &node2) {
                    continue;
                }
                pairs.push_back(std::make_pair(&node1, &node2));
            }
        }
    }
    //This may happen in theory, but hopefully not in our case. We want to be able to draw for up to amount=1.0f
    tgtAssert(pairs.size() > input.getEdges().size(), "Less insert candidates than edges!");

    auto forked_re = random_engine;
    auto selected_pairs = draw_n(std::move(pairs), std::floor(amount*input.getEdges().size()), forked_re);

    for(auto pair: selected_pairs) {
        const VesselGraphNode& node1 = *pair.first;
        const VesselGraphNode& node2 = *pair.second;

        float distance = tgt::distance(node1.pos_, node2.pos_);
        output->insertEdge(node1.getID(), node2.getID(), edgeGenerator.generateEdgePathProperties(random_engine, distance));
    }
    return output;
}

struct NodeSplitEdgeConfiguration {
    size_t node1_id;
    size_t node2_id;
    NodeSplitEdgeConfiguration(const VesselGraphEdge& edge)
        : node1_id(edge.getNodeID1())
        , node2_id(edge.getNodeID2())
    {
    }
};

static std::unique_ptr<VesselGraph> splitNodes(const VesselGraph& input, float amount, random_engine_type random_engine) {

    auto nodes_to_split = draw_nodes_if(input.getNodes(), amount, [] (const VesselGraphNode& n) {
            return n.getDegree() > 2;
            }, random_engine);

    std::unordered_set<const VesselGraphNode*> nodes_to_split_lookup(nodes_to_split.begin(), nodes_to_split.end());

    std::unique_ptr<VesselGraph> output(new VesselGraph());
    for(auto& node : input.getNodes()) {
        output->insertNode(node);
    }

    std::unordered_map<const VesselGraphEdge*, NodeSplitEdgeConfiguration> split_off_edges;
    for(auto node_ptr : nodes_to_split) {
        const auto& node = *node_ptr;
        //TODO: we should consider to "nudge" the node a little bit, if we want to support splitting with
        //other configurations than n=2+s (see below).
        size_t split_node_id = output->insertNode(node);

        auto edges_to_split_off = draw_n(node.getEdges(), 2, random_engine);

        for(auto edge_ref: edges_to_split_off) {
            const auto& edge = edge_ref.get();
            if(split_off_edges.count(&edge) == 0) {
                split_off_edges.insert(std::make_pair(&edge, NodeSplitEdgeConfiguration(edge)));
            }
            NodeSplitEdgeConfiguration& edgeconf = split_off_edges.at(&edge);
            if(edge.isLoop()) {
                if(std::uniform_int_distribution<int>(0,1)(random_engine) == 0) {
                    // The edge is a loop, so getNodeID1() == getNodeID2().
                    // Therefore, we just choose one of the "ends" of the edge to try to replace it with the newly
                    // generated node id, and if that fails we choose the other.
                    if(edgeconf.node1_id != split_node_id) {
                        edgeconf.node1_id = split_node_id;
                    } else {
                        tgtAssert(edgeconf.node2_id != split_node_id, "something is wrong with node splitting (loop)");
                        edgeconf.node2_id = split_node_id;
                    }
                } else {
                    if(edgeconf.node2_id != split_node_id) {
                        edgeconf.node2_id = split_node_id;
                    } else {
                        tgtAssert(edgeconf.node1_id != split_node_id, "something is wrong with node splitting (loop)");
                        edgeconf.node1_id = split_node_id;
                    }
                }
            } else {
                // The edge is NOT a loop, so we know getNodeID1() == getNodeID2().
                // Thus, the edge is only connected to this node once, and we search for the node id that
                // matches the id of the node to be split.
                if(edgeconf.node1_id == node.getID()) {
                    edgeconf.node1_id = split_node_id;
                } else {
                    tgtAssert(edgeconf.node2_id == node.getID(), "something is wrong with node splitting");
                    edgeconf.node2_id = split_node_id;
                }
            }
        }
    }

    for(auto& edge : input.getEdges()) {
        size_t node1_id, node2_id;
        if(split_off_edges.count(&edge) > 0) {
            NodeSplitEdgeConfiguration& edgeconf = split_off_edges.at(&edge);
            node1_id = edgeconf.node1_id;
            node2_id = edgeconf.node2_id;
        } else {
            node1_id = edge.getNodeID1();
            node2_id = edge.getNodeID2();
        }
        std::vector<VesselSkeletonVoxel> path(edge.getVoxels());
        output->insertEdge(node1_id, node2_id, std::move(path));
    }
    return VesselGraphNormalization::removeDregree2Nodes(*output);
}

struct ContinuousPathPos {
    int index;
    float interNodePos;
    const VesselGraphEdge& edge;

    //TODO: We're not really sampling splits evenly across the length of the edge, as the
    //      distance between nodes is not constant.
    ContinuousPathPos(const VesselGraphEdge& edge, random_engine_type& random_engine)
        : index(std::uniform_int_distribution<int>(-1, edge.getVoxels().size()-1)(random_engine))
        , interNodePos(std::uniform_real_distribution<float>(
                0 + std::numeric_limits<float>::epsilon(),    // avoid zero length edges
                1 - std::numeric_limits<float>::epsilon() // also for 1
                )(random_engine))
        , edge(edge)
    {
    }

    tgt::vec3 splitLocation() const {
        const auto& path(edge.getVoxels());
        int path_size = path.size();
        tgt::vec3 split_pos_base_l = (index == -1)          ? edge.getNode1().pos_ : path.at(index).pos_;
        tgt::vec3 split_pos_base_r = (index+1 == path_size) ? edge.getNode2().pos_ : path.at(index+1).pos_;

        return split_pos_base_l*(1-interNodePos) + split_pos_base_r*(interNodePos); //mix
    }

    std::vector<VesselSkeletonVoxel> splitPathLeft() const {
        const auto& path = edge.getVoxels();
        return std::vector<VesselSkeletonVoxel>(path.begin(), path.begin() + index + 1);
    }

    std::vector<VesselSkeletonVoxel> splitPathRight() const {
        const auto& path = edge.getVoxels();
        return std::vector<VesselSkeletonVoxel>(path.begin() + index + 1, path.end());
    }

    bool operator<(const ContinuousPathPos& other) const {
        tgtAssert(&edge == &other.edge, "Comparing paths from different edges");
        if(index != other.index) {
            return index < other.index;
        } else {
            return interNodePos < other.interNodePos;
        }
    }
};

static std::unique_ptr<VesselGraph> subdivideEdges(const VesselGraph& input, float amount, random_engine_type random_engine) {
    const auto& input_edges = input.getEdges();
    std::vector<const VesselGraphEdge*> edges_to_subdivide = draw_edges(input_edges, amount, random_engine);
    std::unordered_set<const VesselGraphEdge*> edges_to_subdivide_lookup(edges_to_subdivide.begin(), edges_to_subdivide.end());

    std::unique_ptr<VesselGraph> output(new VesselGraph());
    for(auto& node : input.getNodes()) {
        output->insertNode(node);
    }

    // First: Insert all non-subdivided edges
    for(auto& edge : input.getEdges()) {
        if(edges_to_subdivide_lookup.count(&edge) == 0) {
            // Note: We ware using the fact that we are iterating over nodes (and inserted them) in order of ID!
            //       The same applies below.
            output->insertEdge(edge.getNodeID1(), edge.getNodeID2(), edge.getPathProperties());
        }
    }

    // Now: Subdivide selected edges and insert resulting edges
    for(auto edge_ptr : edges_to_subdivide) {
        auto& edge = *edge_ptr;
        const auto& path(edge.getVoxels());

        ContinuousPathPos split(edge, random_engine);
        tgt::vec3 split_pos = split.splitLocation();

        size_t split_node_id = output->insertNode(split_pos, std::vector<tgt::vec3>(), false);

        // First path part
        auto split_left = split.splitPathLeft();
        if(std::count_if(split_left.begin(), split_left.end(), [] (const VesselSkeletonVoxel& v) { return v.hasValidData(); }) > 0) {
            output->insertEdge(edge.getNodeID1(), split_node_id, std::move(split_left));
        } else {
            VesselGraphEdgePathProperties properties = edge.getPathProperties();
            // Here (and below for the second path) we can really just guess that most of the properties
            // are the same as in the rest of the edge/vessel segment
            properties.length_ = tgt::distance(edge.getNode1().pos_, split_pos);
            properties.volume_ = properties.length_*edge.getAvgCrossSection();
            output->insertEdge(edge.getNodeID1(), split_node_id, properties);
        }

        // Second path part
        auto split_right = split.splitPathRight();
        if(std::count_if(split_left.begin(), split_left.end(), [] (const VesselSkeletonVoxel& v) { return v.hasValidData(); }) > 0) {
            output->insertEdge(split_node_id, edge.getNodeID2(), std::move(split_right));
        } else {
            VesselGraphEdgePathProperties properties = edge.getPathProperties();
            // See above
            properties.length_ = tgt::distance(split_pos, edge.getNode2().pos_);
            properties.volume_ = properties.length_*edge.getAvgCrossSection();
            output->insertEdge(split_node_id, edge.getNodeID2(), properties);
        }

        // Generate the edge that subdivides the first
        float new_length = std::abs(normal_distribution(0, edge.getDistance()/2)(random_engine))
                            + std::numeric_limits<float>::epsilon(); // has to be > 0

        tgt::vec3 new_direction = generate_random_direction(random_engine);

        tgt::vec3 new_node_pos = split_pos + new_direction*new_length;
        size_t new_node_id = output->insertNode(new_node_pos, std::vector<tgt::vec3>(), false);

        VesselGraphEdgePathProperties new_edge_properties = edge.getPathProperties();
        // Again, we assume that radius, roundness, etc. do not change
        // TODO: Maybe we want to randomize these using EdgeGenerator?
        new_edge_properties.length_ = new_length;
        new_edge_properties.volume_ = new_edge_properties.length_*edge.getAvgCrossSection();
        //tgtAssert(new_edge_properties.hasValidData(), "invalid new edge data");
        size_t inserted = output->insertEdge(split_node_id, new_node_id, new_edge_properties);
    }
    return output;
}

static std::unique_ptr<VesselGraph> splitEdges(const VesselGraph& input, float amount, random_engine_type random_engine) {

    const auto& input_edges = input.getEdges();
    std::vector<const VesselGraphEdge*> edges_to_split = draw_edges(input_edges, amount, random_engine);
    std::unordered_set<const VesselGraphEdge*> edges_to_split_lookup(edges_to_split.begin(), edges_to_split.end());

    std::unique_ptr<VesselGraph> output(new VesselGraph());
    for(auto& node : input.getNodes()) {
        output->insertNode(node);
    }

    // First: Insert all non-split edges
    for(auto& edge : input.getEdges()) {
        if(edges_to_split_lookup.count(&edge) == 0) {
            // Note: We ware using the fact that we are iterating over nodes (and inserted them) in order of ID!
            //       The same applies below.
            output->insertEdge(edge.getNodeID1(), edge.getNodeID2(), edge.getPathProperties());
        }
    }

    // Now: Subdivide selected edges and insert resulting edges
    for(auto edge_ptr : edges_to_split) {
        auto& edge = *edge_ptr;
        const auto& path(edge.getVoxels());

        ContinuousPathPos split1(edge, random_engine);
        ContinuousPathPos split2(edge, random_engine);

        ContinuousPathPos& split_l = (split1 < split2) ? split1: split2;
        ContinuousPathPos& split_r = (split1 < split2) ? split2: split1;

        {
            // First path part
            tgt::vec3 split_pos_l = split_l.splitLocation();
            size_t split_node_id_l = output->insertNode(split_pos_l, std::vector<tgt::vec3>(), false);

            auto path_left = split_l.splitPathLeft();
            if(path_left.empty()) {
                VesselGraphEdgePathProperties properties = edge.getPathProperties();
                // Here (and below for the second path) we can really just guess that most of the properties
                // are the same as in the rest of the edge/vessel segment
                properties.length_ = tgt::distance(edge.getNode1().pos_, split_pos_l);
                properties.volume_ = properties.length_*edge.getAvgCrossSection();
                output->insertEdge(edge.getNodeID1(), split_node_id_l, properties);
            } else {
                output->insertEdge(edge.getNodeID1(), split_node_id_l, std::move(path_left));
            }
        }

        {
            // Second path part
            tgt::vec3 split_pos_r = split_r.splitLocation();
            size_t split_node_id_r = output->insertNode(split_pos_r, std::vector<tgt::vec3>(), false);

            auto path_right = split_r.splitPathRight();
            if(path_right.empty()) {
                VesselGraphEdgePathProperties properties = edge.getPathProperties();
                // See above
                properties.length_ = tgt::distance(split_pos_r, edge.getNode2().pos_);
                properties.volume_ = properties.length_*edge.getAvgCrossSection();
                output->insertEdge(split_node_id_r, edge.getNodeID2(), properties);
            } else {
                output->insertEdge(split_node_id_r, edge.getNodeID2(), std::move(path_right));
            }
        }
    }
    return output;
}

static std::unique_ptr<VesselGraph> moveNodes(const VesselGraph& input, float amount, random_engine_type random_engine) {
    float length_sum = 0;
    for(auto& edge : input.getEdges()) {
        length_sum += edge.getLength();
    }
    float length_avg = length_sum / input.getEdges().size();

    float perturbation = 2*amount*length_avg;

    std::unique_ptr<VesselGraph> output(new VesselGraph());
    for(auto& node : input.getNodes()) {
        // Generate the edge that subdivides the first
        float shift_length = std::abs(normal_distribution(0, perturbation/2)(random_engine));

        tgt::vec3 shift = shift_length * generate_random_direction(random_engine);

        std::vector<tgt::vec3> voxels;

        for(auto& v : node.voxels_) {
            voxels.push_back(v + shift);
        }

        output->insertNode(node.pos_+ shift, std::move(voxels), node.isAtSampleBorder_);
    }

    for(auto& edge : input.getEdges()) {
        output->insertEdge(edge.getNodeID1(), edge.getNodeID2(), edge);
    }
    return output;
}

static std::unique_ptr<VesselGraph> changeProperties(const VesselGraph& input, float amount, random_engine_type random_engine) {
    float length_sum = 0;
    for(auto& edge : input.getEdges()) {
        length_sum += edge.getLength();
    }
    float length_avg = length_sum / input.getEdges().size();

    float perturbation = 2*amount*length_avg;

    std::unique_ptr<VesselGraph> output(new VesselGraph());
    for(auto& node : input.getNodes()) {
        output->insertNode(node);
    }

    for(auto& edge : input.getEdges()) {
        auto properties = modifyEdgePathProperties(random_engine, amount, edge.getPathProperties());
        output->insertEdge(edge.getNodeID1(), edge.getNodeID2(), properties);
    }
    return output;
}

void VesselGraphPerturbation::process() {
    const VesselGraph* input = inport_.getData();
    if(!input) {
        outport_.setData(nullptr);
        return;
    }
    if(!enabled_.get()) {
        outport_.setData(input, false);
        return;
    }
    random_engine_type randomEngine(randomDevice());
    if(usePredeterminedSeed_.get()) {
        randomEngine.seed(predeterminedSeed_.get());
    }

    std::unique_ptr<VesselGraph> output(nullptr);
    switch(perturbationMethod_.getValue()) {
        case ADD_EDGES: // See NetMets: software for quantifying [...] network segmentation, figure 4 (a)
            output = addEdges(*input, perturbationAmount_.get(), randomEngine);
            break;
        case SPLIT_NODES: // ... (b)
            output = splitNodes(*input, perturbationAmount_.get(), randomEngine);
            break;
        case SUBDIVIDE_EDGES: // ... (c)
            output = subdivideEdges(*input, perturbationAmount_.get(), randomEngine);
            break;
        case SPLIT_EDGES: // not shown, cut an edge in half, adding two nodes: o-------o -> o--o o--o
            output = splitEdges(*input, perturbationAmount_.get(), randomEngine);
            break;
        case MOVE_NODES:
            output = moveNodes(*input, perturbationAmount_.get(), randomEngine);
            break;
        case CHANGE_PROPERTIES:
            output = changeProperties(*input, perturbationAmount_.get(), randomEngine);
            break;
    }
    tgtAssert(output, "No output graph generated");

    outport_.setData(output.release());
}
} // namespace voreen