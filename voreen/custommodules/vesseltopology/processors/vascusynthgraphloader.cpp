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

#include "vascusynthgraphloader.h"

#include "voreen/core/datastructures/callback/lambdacallback.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/optional.hpp>

namespace voreen {



const std::string VascuSynthGraphLoader::loggerCat_("voreen.vascusynthgraphloader");


VascuSynthGraphLoader::VascuSynthGraphLoader()
    : Processor()
    , outport_(Port::OUTPORT, "graph.output", "Normalized Graph Output", false, Processor::VALID)
    , graphFilePath_("graphFilePath", "VascuSynth Graph File", "Graph File", "", "*.xml", FileDialogProperty::OPEN_FILE)
{
    addPort(outport_);

    addProperty(graphFilePath_);
}

VascuSynthGraphLoader::~VascuSynthGraphLoader() {
}

boost::optional<float> readFloat(const TiXmlElement* element) {
    if(!element) {
        return boost::none;
    }
    const char* text = element->GetText();
    if(!text) {
        return boost::none;
    }
    return ::atof(text);
}

// This is very specific to the vascusynth graph files!
boost::optional<size_t> readId(const TiXmlElement* element, const std::string& name) {
    if(!element) {
        return boost::none;
    }
    const char* id_str = element->Attribute(name.c_str());
    if(!id_str || *id_str == 0) {
        return boost::none;
    }
    return ::atoi(id_str+1); //Again, very specific to vascusynth files
}

void VascuSynthGraphLoader::process() {
    const std::string path = graphFilePath_.get();
    if(path.empty()) {
        return;
    }
    std::unique_ptr<VesselGraph> output(nullptr);

    TiXmlDocument doc(path);
    if(doc.LoadFile()) {
        const TiXmlNode* glxNode = doc.FirstChild();
        if(!glxNode) {
            LERROR("Could not find glxNode");
        }
        const TiXmlNode* graphNode = glxNode->FirstChild();
        if(!graphNode) {
            LERROR("Could not find graphNode");
        }
        output.reset(new VesselGraph());
        std::map<size_t, size_t> idMap;
        for(const TiXmlElement* graphElement = graphNode->FirstChildElement(); graphElement; graphElement = graphElement->NextSiblingElement()) {
            if(std::string(graphElement->Value()) == "node") {
                auto maybeNodeId = readId(graphElement, "id");
                if(!maybeNodeId) {
                    LERROR("No node id");
                }
                size_t nodeId = *maybeNodeId;
                for(const TiXmlElement* attributeElement = graphElement->FirstChildElement(); attributeElement; attributeElement = attributeElement->NextSiblingElement()) {
                    const char* attrName = attributeElement->Attribute("name");
                    if(attrName && std::string(attrName) == " position") {
                        const TiXmlElement* tup = attributeElement->FirstChildElement();
                        if(!tup) {
                            LERROR("No tup node");
                            continue;
                        }
                        tgt::vec3 pos;
                        const TiXmlElement* floatElement = tup->FirstChildElement();
                        auto x = readFloat(floatElement);
                        if(!x) {
                            LERROR("No x node position");
                            continue;
                        }
                        pos.x = *x;

                        floatElement = floatElement->NextSiblingElement();
                        auto y = readFloat(floatElement);
                        if(!y) {
                            LERROR("No y node position");
                            continue;
                        }
                        pos.y = *y;

                        floatElement = floatElement->NextSiblingElement();
                        auto z = readFloat(floatElement);
                        if(!z) {
                            LERROR("No z node position");
                            continue;
                        }
                        pos.z = *z;
                        pos /= 1000.0f; // Asuming uniform spacing 0.001mm.
                        if(tgt::isNaN(pos)) {
                            LERROR("pos is nan: " << pos);
                            continue;
                        }
                        std::vector<tgt::vec3> voxels;
                        voxels.push_back(pos);
                        size_t id = output->insertNode(pos, std::move(voxels), false);
                        idMap.insert({nodeId, id});
                    }
                }
            } else if(std::string(graphElement->Value()) == "edge") {
                auto maybeFromId = readId(graphElement, "from");
                if(!maybeFromId) {
                    LERROR("No from id");
                }
                size_t fromId = *maybeFromId;

                auto maybeToId = readId(graphElement, "to");
                if(!maybeToId) {
                    LERROR("No to id");
                }
                size_t toId = *maybeToId;

                for(const TiXmlElement* attributeElement = graphElement->FirstChildElement(); attributeElement; attributeElement = attributeElement->NextSiblingElement()) {
                    const char* attrName = attributeElement->Attribute("name");
                    if(attrName && std::string(attrName) == " radius") {
                        const TiXmlElement* floatElement = attributeElement->FirstChildElement();
                        auto maybeRadius = readFloat(floatElement);
                        if(!maybeRadius) {
                            LERROR("No radius");
                            continue;
                        }
                        float radius = *maybeRadius;
                        try {
                            const size_t graphFromId = idMap.at(fromId);
                            const size_t graphToId = idMap.at(toId);

                            radius /= 1000; //Assuming uniform spacing 0.001mm.

                            VesselGraphEdgePathProperties properties;
                            properties.length_ = tgt::distance(output->getNode(graphFromId).pos_, output->getNode(graphToId).pos_);
                            properties.volume_ = properties.length_*radius*radius;
                            properties.minRadiusAvg_ = radius;
                            properties.minRadiusStdDeviation_ = 0;
                            properties.maxRadiusAvg_ = radius;
                            properties.maxRadiusStdDeviation_ = 0;
                            properties.avgRadiusAvg_ = radius;
                            properties.avgRadiusStdDeviation_ = 0;
                            properties.roundnessAvg_ = 1;
                            properties.roundnessStdDeviation_ = 0;
                            output->insertEdge(graphFromId, graphToId, properties);
                        } catch(...) {
                            LERROR("graph without matching node");
                            continue;
                        }
                    }
                }
            } else {
                LERROR("Unknown element value " << graphElement->Value());
            }
        }
    } else {
        LERROR("Could not load xml file " << path);
    }

    //size_t id = output->insertNode(new_pos, std::move(voxels), false);
    //size_t id = output->insertEdge(future_node_id1, future_node_id2, std::move(voxels));
    outport_.setData(output.release());
}
} // namespace voreen