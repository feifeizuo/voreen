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

#include "interactiveprojectionlabeling.h"
#include "voreen/core/datastructures/volume/volumeram.h"
#include "voreen/core/datastructures/volume/volumefactory.h"
#include "voreen/core/datastructures/octree/volumeoctree.h"
#include <iostream>
#include "tgt/textureunit.h"
#include "tgt/init.h"
#include "tgt/immediatemode/immediatemode.h"

namespace voreen {

const std::string InteractiveProjectionLabeling::loggerCat_("voreen.vesselnetworkanalysisextra.interactiveprojectionlabeling");

LabelGuard::LabelGuard(LabelProjection& labelProjection)
    : labelProjection_(labelProjection)
{
}
LabelGuard::~LabelGuard() {
    labelProjection_.ensureTexturesPresent();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    labelProjection_.projectionTexture_->uploadTexture();
}
tgt::vec2& LabelGuard::at(tgt::svec2 p) {
    return labelProjection_.projection_.voxel(p.x, p.y, 0);
}

LabelProjection::LabelProjection(tgt::svec2 dimensions)
    : projection_(tgt::svec3(dimensions, 1))
    , projectionTexture_(boost::none)
{
    projection_.clear();
}
void LabelProjection::ensureTexturesPresent() {
    if(!projectionTexture_) {
        projectionTexture_ = tgt::Texture(projection_.getDimensions(), GL_RG, GL_RG, GL_FLOAT, tgt::Texture::LINEAR, tgt::Texture::CLAMP_TO_EDGE, (GLubyte*) projection_.voxel(), false);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        projectionTexture_->uploadTexture();
    }
}
void LabelProjection::bindTexture() {
    ensureTexturesPresent();
    projectionTexture_->bind();
}

template<class Vec>
struct PolyLinePoint {
    Vec pos_;
    float d_;
};
template<class Vec>
struct PolyLine {
    PolyLine(std::deque<Vec>& points)
        : points_()
    {
        tgtAssert(!points.empty(), "Points must not be empty!")
        float len = 0.0;
        for(int i=0; i<((int)points.size())-1; ++i) {
            len += tgt::distance(points[i], points[i+1]);
        }
        float norm_len = 0.0;
        for(int i=0; i<((int)points.size())-1; ++i) {
            points_.push_back( PolyLinePoint<Vec> {
                    points[i],
                    norm_len
                    });
            norm_len += tgt::distance(points[i], points[i+1])/len;
        }
        points_.push_back( PolyLinePoint<Vec> {
                points.back(),
                norm_len
                });
    }

    std::deque<PolyLinePoint<Vec>> points_;

    tgt::vec2 interpolate(float d) const {
        tgtAssert(0.0 <= d && d <= 1.0, "Invalid interpolation parameter");
        if(points_.size() == 1) {
            return points_[0].pos_;
        }
        int i=0;
        while(d > points_[i+1].d_ && i < ((int)points_.size())-2) {
            ++i;
        }
        auto& p1 = points_[i];
        auto& p2 = points_[i+1];
        if(p1.d_ == p2.d_) {
            return p1.pos_;
        }
        float alpha = (d - p1.d_) / (p2.d_- p1.d_);
        tgt::vec2 res = p1.pos_ * (1-alpha) + alpha * p2.pos_;
        tgtAssert(std::isfinite(res.x) && std::isfinite(res.y), "Invalid interpolation result");
        return res;
    }
};

struct Line {
    Line(tgt::vec2 p1, tgt::vec2 p2)
        : p1_(p1)
        , p2_(p2)
    {
    }
    tgt::vec2 p1_;
    tgt::vec2 p2_;

    float len() {
        return tgt::distance(p1_, p2_);
    }
    float dist(tgt::vec2 q) {
        auto parallel = p1_-p2_;
        auto parallel_norm = tgt::normalize(parallel);

        float along = tgt::dot(parallel_norm, q - p2_);
        if(0 > along || along > len()) {
            return std::numeric_limits<float>::infinity();
        }

        tgt::vec2 orthogonal(parallel.y, -parallel.x);
        auto orth_norm = tgt::normalize(orthogonal);
        float dist = tgt::dot(orth_norm, p1_-q);
        return tgt::abs(dist);
    }
};

void ProjectionLabels::clear() {
    foreground_.clear();
    background_.clear();
}

static tgt::vec2 projectionDepthRange(const VolumeBase& vol, const VolumeAtomic<tgt::vec4>& front, const VolumeAtomic<tgt::vec4>& back, PolyLine<tgt::vec2>& line, tgt::vec3 camera) {

    auto dim = vol.getDimensions();

    auto tex_to_world = vol.getTextureToWorldMatrix();

    float min_dist = std::numeric_limits<float>::infinity();
    float max_dist = 0.0f;
    for(int x = 0; x < dim.x; ++x) {
        float d = ((float)x)/(dim.x-1);
        auto p = line.interpolate(d);

        tgt::vec3 normalized_query(p, 0);
        tgt::vec4 front_pos = front.getVoxelLinear(normalized_query * tgt::vec3(front.getDimensions()));
        tgt::vec4 back_pos = back.getVoxelLinear(normalized_query * tgt::vec3(back.getDimensions()));

        if(front_pos.a > 0) {
            min_dist = std::min(min_dist, tgt::distance(camera, (tex_to_world * front_pos).xyz()));
        }
        if(back_pos.a > 0) {
            max_dist = std::max(max_dist, tgt::distance(camera, (tex_to_world * back_pos).xyz()));
        }
    }

    return tgt::vec2(min_dist, max_dist);
}


#define MOUSE_INTERACTION_DIST 0.02f
static bool handleLineEvent(std::deque<tgt::vec2>& points, tgt::MouseEvent* e) {
    auto button = e->button();
    if((button & (tgt::MouseEvent::MOUSE_BUTTON_LEFT | tgt::MouseEvent::MOUSE_BUTTON_RIGHT)) == 0) {
        return false;
    }

    tgt::ivec2 coords = e->coord();
    tgt::ivec2 viewport = e->viewport();

    coords.y = viewport.y - coords.y - 1;
    auto mouse = tgt::vec2(coords)/tgt::vec2(viewport);

    boost::optional<int> nearest = boost::none;
    int i = 0;
    for(auto& p : points) {
        float dist = tgt::distance(p, mouse);
        if (dist < MOUSE_INTERACTION_DIST && (!nearest || dist < tgt::distance(points[i], mouse))) {
            nearest = i;
        }
        ++i;
    }
    if(nearest) {
        if(e->action() == tgt::MouseEvent::RELEASED && button == tgt::MouseEvent::MOUSE_BUTTON_RIGHT) {
            points.erase(points.begin() + *nearest);
        } else {
            points[*nearest] = mouse;
        }
    } else if(e->action() == tgt::MouseEvent::RELEASED && button == tgt::MouseEvent::MOUSE_BUTTON_LEFT) {
        if(points.empty()) {
            points.push_back(mouse);
            e->accept();
            return true;
        }

        int insert_index = -1;
        float nearest_dist = std::numeric_limits<float>::infinity();
        for(int i=0; i<((int)points.size())-1; ++i) {
            Line line(points[i], points[i+1]);
            float dist = line.dist(mouse);
            if(dist < nearest_dist) {
                insert_index = i;
                nearest_dist = dist;
            }
        }
        //Ok since points is not empty:
        float front_dist = tgt::distance(points.front(), mouse);
        float back_dist = tgt::distance(points.back(), mouse);
        if(front_dist <= back_dist && front_dist < nearest_dist) {
            points.push_front(mouse);
        } else if(back_dist <= front_dist && back_dist < nearest_dist) {
            points.push_back(mouse);
        } else {
            tgtAssert(insert_index != -1, "Invalid insert index");
            points.insert(points.begin() + insert_index+1, mouse);
        }
    }
    e->accept();
    return true;
}

boost::optional<tgt::vec2> handleProjectionEvent(tgt::MouseEvent* e, ProjectionLabels& labels, const boost::optional<tgt::vec2>& prevMousePos) {
    auto button = e->button();

    tgt::ivec2 coords = e->coord();
    tgt::ivec2 viewport = e->viewport();

    struct NearestNode {
        std::deque<tgt::vec2>* line;
        int index;
    };

    coords.y = viewport.y - coords.y - 1;
    auto mouse = tgt::vec2(coords)/tgt::vec2(viewport);
    {
        auto findNearestNodeTo = [&] (const tgt::vec2& mousePos) {
            float nearest_dist = MOUSE_INTERACTION_DIST;
            boost::optional<NearestNode> nearest = boost::none;
            auto findNearestNodeInLine = [&] (std::deque<tgt::vec2>& line) {
                int i = 0;
                for(auto& p : line) {
                    float dist = tgt::distance(p, mousePos);
                    if (dist < MOUSE_INTERACTION_DIST && (!nearest || dist < nearest_dist)) {
                        nearest = NearestNode {
                            &line, i
                        };
                        nearest_dist = dist;
                    }
                    ++i;
                }
            };
            for(auto& line : labels.foreground_) {
                findNearestNodeInLine(line);
            }
            for(auto& line : labels.background_) {
                findNearestNodeInLine(line);
            }
            return nearest;
        };

        if(e->action() == tgt::MouseEvent::RELEASED && button == tgt::MouseEvent::MOUSE_BUTTON_RIGHT) {
            auto nearest = findNearestNodeTo(mouse);
            if(nearest) {
                nearest->line->erase(nearest->line->begin() + nearest->index);
                labels.foreground_.erase(std::remove_if(labels.foreground_.begin(),
                            labels.foreground_.end(),
                            [](std::deque<tgt::vec2>& q){ return q.empty(); }), labels.foreground_.end());
                labels.background_.erase(std::remove_if(labels.background_.begin(),
                            labels.background_.end(),
                            [](std::deque<tgt::vec2>& q){ return q.empty(); }), labels.background_.end());

                e->accept();
                return boost::none;
            }
        } else if(button == tgt::MouseEvent::MOUSE_BUTTON_LEFT) {
            auto nearest = findNearestNodeTo(prevMousePos ? *prevMousePos : mouse);
            if(nearest) {
                nearest->line->at(nearest->index) = mouse;
                e->accept();
                if(e->action() == tgt::MouseEvent::PRESSED || e->action() == tgt::MouseEvent::MOTION) {
                    return mouse;
                } else {
                    return boost::none;
                }
            }
        }
    }
    if(e->action() == tgt::MouseEvent::PRESSED && button == tgt::MouseEvent::MOUSE_BUTTON_MIDDLE) {
        struct NearestSegment {
            std::vector<std::deque<tgt::vec2>>::iterator line;
            int split_pos;
        };
        float nearest_dist = std::numeric_limits<float>::infinity();
        boost::optional<NearestSegment> nearest = boost::none;

        auto findSplitPos = [&] (std::vector<std::deque<tgt::vec2>>::iterator points) {
            for(int i=0; i<((int)points->size())-1; ++i) {
                Line line((*points)[i], (*points)[i+1]);
                float dist = line.dist(mouse);
                if(dist < nearest_dist) {
                    nearest = NearestSegment {
                        points,
                        i+1 // insert betweeen points[i] and points[i+1]
                    };
                    nearest_dist = dist;
                }
            }
        };

        for(auto it = labels.foreground_.begin(); it != labels.foreground_.end(); ++it) {
            findSplitPos(it);
        }
        for(auto it = labels.background_.begin(); it != labels.background_.end(); ++it) {
            findSplitPos(it);
        }

        if(nearest) {
            std::deque<tgt::vec2> right;
            for(int i=nearest->split_pos; i < nearest->line->size(); ++i) {
                tgt::vec2 elm = nearest->line->at(i);
                right.push_back(elm);
            }
            nearest->line->resize(nearest->split_pos);
            if(labels.foreground_.begin() <= nearest->line && nearest->line < labels.foreground_.end()) {
                labels.foreground_.push_back(std::move(right));
            } else {
                tgtAssert(labels.background_.begin() <= nearest->line && nearest->line < labels.background_.end() , "Invalid label list pointer");
                labels.background_.push_back(std::move(right));
            }
        }
    } else if(e->action() == tgt::MouseEvent::PRESSED && button == tgt::MouseEvent::MOUSE_BUTTON_LEFT) {
        float nearest_dist = std::numeric_limits<float>::infinity();
        boost::optional<NearestNode> nearest = boost::none;

        auto findNewNodeInsertPos = [&] (std::deque<tgt::vec2>& points) {
            int insert_index = -1;

            if(points.size() == 1) {
                // Special case lines with only one element
                nearest = NearestNode {
                    &points,
                    1 // at end
                };
                nearest_dist = 0;
                return;
            }

            for(int i=0; i<((int)points.size())-1; ++i) {
                Line line(points[i], points[i+1]);
                float dist = line.dist(mouse);
                if(dist < nearest_dist) {
                    nearest = NearestNode {
                        &points,
                        i+1 // insert betweeen points[i] and points[i+1]
                    };
                    nearest_dist = dist;
                }
            }
            //Ok since points is not empty:
            float front_dist = tgt::distance(points.front(), mouse);
            float back_dist = tgt::distance(points.back(), mouse);
            if(front_dist < nearest_dist) {
                nearest = NearestNode {
                    &points,
                    0, // insert at the very beginning
                };
                nearest_dist = front_dist;
            }
            if(back_dist < nearest_dist) {
                nearest = NearestNode {
                    &points,
                    (int)points.size(), // insert at the very end
                };
                nearest_dist = back_dist;
            }
        };

        for(auto& line : labels.foreground_) {
            findNewNodeInsertPos(line);
        }
        for(auto& line : labels.background_) {
            findNewNodeInsertPos(line);
        }
        if(nearest) {
            nearest->line->insert(nearest->line->begin() + nearest->index, mouse);

            e->accept();
            return mouse;
        }
    }
    return boost::none;
}

static void zoomPoint(float& point, float center, float factor) {
    point = center + (point - center)*factor;
}

static void zoomProjection(LabelUnit& unit, float center, float factor) {
    tgtAssert(factor > 0.0f, "Zoom factor must be larger than 0");

    tgt::vec2 range = unit.getZoomRegion();
    float range_center = range.y * center + (1.0f - center) * range.x;

    zoomPoint(range.x, range_center, factor);
    zoomPoint(range.y, range_center, factor);

    range.x = std::max(0.0f, range.x);
    range.y = std::min(1.0f, range.y);

    unit.setZoomRegion(range);
}

void InteractiveProjectionLabeling::projectionEvent(tgt::MouseEvent* e) {
    auto button = e->button();

    tgt::ivec2 coords = e->coord();
    tgt::ivec2 viewport = e->viewport();

    coords.y = viewport.y - coords.y - 1;
    auto mouse = tgt::vec2(coords)/tgt::vec2(viewport);

    if(button == tgt::MouseEvent::MOUSE_WHEEL_UP && e->modifiers() == tgt::Event::MODIFIER_NONE) {
        // Zoom in
        zoomProjection(currentUnit(), mouse.y, 0.9);
        projectionRequiresUpdate_ = true;
        invalidate();
        return;
    } else if(button == tgt::MouseEvent::MOUSE_WHEEL_DOWN && e->modifiers() == tgt::Event::MODIFIER_NONE) {
        zoomProjection(currentUnit(), mouse.y, 1.0/0.9);
        // Zoom out
        projectionRequiresUpdate_ = true;
        invalidate();
        return;
    }

    if(button == tgt::MouseEvent::MOUSE_BUTTON_LEFT && e->modifiers() == tgt::Event::CTRL && e->action() == tgt::MouseEvent::RELEASED) {
        currentUnit().projectionLabels_.foreground_.push_back({mouse});
    } else if(button == tgt::MouseEvent::MOUSE_BUTTON_LEFT && e->modifiers() == tgt::Event::SHIFT && e->action() == tgt::MouseEvent::RELEASED) {
        currentUnit().projectionLabels_.background_.push_back({mouse});
    } else if(e->modifiers() == tgt::Event::MODIFIER_NONE &&
            (button & (tgt::MouseEvent::MOUSE_BUTTON_LEFT | tgt::MouseEvent::MOUSE_BUTTON_RIGHT | tgt::MouseEvent::MOUSE_BUTTON_MIDDLE)) != 0) {
        if((e->action() & tgt::MouseEvent::ENTER_EXIT) == 0) {
            prevProjectionMousePos_ = handleProjectionEvent(e, currentUnit().projectionLabels_, prevProjectionMousePos_);
        } else {
            prevProjectionMousePos_ = boost::none;
        }
    } else {
        return;
    }
    projectionLabelsModified_ = true;

    invalidate();
}

void InteractiveProjectionLabeling::overlayEvent(tgt::MouseEvent* e) {
    auto button = e->button();

    if(e->modifiers() != tgt::Event::CTRL
            || ((button & (tgt::MouseEvent::MOUSE_BUTTON_LEFT | tgt::MouseEvent::MOUSE_BUTTON_RIGHT)) == 0))
             {
        return;
    }

    if(handleLineEvent(currentUnit().displayLine_, e)) {
        // path modified
        projectionRequiresUpdate_ = true;
    }

    invalidate();
}

void InteractiveProjectionLabeling::onPortEvent(tgt::Event* e, Port* port) {
    if(tgt::MouseEvent* me = dynamic_cast<tgt::MouseEvent*>(e)) {
        if(port == &overlayOutput_) {
            overlayEvent(me);
            if(!currentUnit().displayLine_.empty() && state_ == FREE) {
                state_ = LABELING;
                projectionLabelsModified_ = false;
            }
            if(state_ == LABELING) {
                currentUnit().camera_ = camera_.get();
                me->accept();
            }
        }
        else if(port == &projectionOutput_) {
            projectionEvent(me);

            // Definitely consume events for this port.
            me->accept();
        }
    } else if(tgt::KeyEvent* ke = dynamic_cast<tgt::KeyEvent*>(e)) {
        if(ke->pressed()) {
            switch(ke->keyCode()) {
                case tgt::KeyEvent::K_UP: {
                    int new_val = currentUnitIndex_.get() + 1;
                    if(new_val > currentUnitIndex_.getMaxValue()) {
                        new_val = -1;
                    }
                    currentUnitIndex_.set(new_val);
                    ke->accept();
                    break;
                }
                case tgt::KeyEvent::K_DOWN: {
                    int new_val = currentUnitIndex_.get() - 1;
                    if(new_val < currentUnitIndex_.getMinValue()) {
                        new_val = currentUnitIndex_.getMaxValue();
                    }
                    currentUnitIndex_.set(new_val);
                    ke->accept();
                    break;
                }
                case tgt::KeyEvent::K_DELETE: {
                    auto index = currentUnitIndex();
                    if(index) {
                        labelUnits_.erase(labelUnits_.begin() + *index);
                        currentUnitIndex_.setMaxValue(labelUnits_.size()-1);
                    }
                    seedsChanged_ = true;
                    resetCurrentUnit();
                    state_ = FREE;
                    ke->accept();
                    invalidate();
                    break;
                }
                case tgt::KeyEvent::K_ESCAPE: {
                    resetCurrentUnit();
                    state_ = FREE;
                    ke->accept();
                    invalidate();
                    break;
                }
                case tgt::KeyEvent::K_SPACE: {
                    if(state_ == LABELING && !currentUnit().projectionLabels_.foreground_.empty()) {
                        finishProjection();
                        auto index = currentUnitIndex();
                        if(index) {
                            labelUnits_.at(*index) = std::move(currentUnit());
                        } else {
                            labelUnits_.push_back(std::move(currentUnit()));
                            currentUnitIndex_.setMaxValue(labelUnits_.size()-1);
                        }
                        resetCurrentUnit();
                        state_ = FREE;
                        invalidate();
                    }
                    ke->accept();
                    break;
                }
                case tgt::KeyEvent::K_1: {
                    initializationMode_.selectByValue(NONE);
                    ke->accept();
                    break;
                }
                case tgt::KeyEvent::K_2: {
                    initializationMode_.selectByValue(BRIGHT_LUMEN);
                    ke->accept();
                    break;
                }
                case tgt::KeyEvent::K_3: {
                    initializationMode_.selectByValue(BRIGHT_WALL);
                    ke->accept();
                    break;
                }
                case tgt::KeyEvent::K_Q: {
                    float new_val = tgt::clamp(
                            maxLineSimplificationDistance_.get() - 0.01f,
                            maxLineSimplificationDistance_.getMinValue(),
                            maxLineSimplificationDistance_.getMaxValue()
                            );
                    maxLineSimplificationDistance_.set(new_val);
                    ke->accept();
                    break;
                }
                case tgt::KeyEvent::K_W: {
                    float new_val = tgt::clamp(
                            maxLineSimplificationDistance_.get() + 0.01f,
                            maxLineSimplificationDistance_.getMinValue(),
                            maxLineSimplificationDistance_.getMaxValue()
                            );
                    maxLineSimplificationDistance_.set(new_val);
                    ke->accept();
                    break;
                }
                case tgt::KeyEvent::K_A: {
                    float new_val = tgt::clamp(
                            backgroundLineDistanceMultiplier_.get() - 0.1f,
                            backgroundLineDistanceMultiplier_.getMinValue(),
                            backgroundLineDistanceMultiplier_.getMaxValue()
                            );
                    backgroundLineDistanceMultiplier_.set(new_val);
                    ke->accept();
                    break;
                }
                case tgt::KeyEvent::K_S: {
                    float new_val = tgt::clamp(
                            backgroundLineDistanceMultiplier_.get() + 0.1f,
                            backgroundLineDistanceMultiplier_.getMinValue(),
                            backgroundLineDistanceMultiplier_.getMaxValue()
                            );
                    backgroundLineDistanceMultiplier_.set(new_val);
                    ke->accept();
                    break;
                }
                default:;
            }
        }
    }
    if(!e->isAccepted() && state_ == FREE) {
        RenderProcessor::onPortEvent(e, port);
    }
}

void InteractiveProjectionLabeling::finishProjection() {
    if(!inport_.hasData()) {
        return;
    }

    auto& current = currentUnit();

    // Configuration that created these results
    current.camera_ = camera_.get();
    current.clippingRegion_ = clippingRegion_.get();

    const auto& vol = *inport_.getData();

    const int NUM_SAMPLES = 100;

    PolyLine<tgt::vec2> displayLine(current.displayLine_);

    auto maybe_front = getFhp();
    auto maybe_back = getLhp();
    if(!maybe_front || !maybe_back) {
        return;
    }
    auto& front = *maybe_front;
    auto& back = *maybe_back;

    tgt::vec3 camera = current.camera_.getPosition();

    auto tex_to_world = vol.getTextureToWorldMatrix();

    auto minmax = projectionDepthRange(vol, front, back, displayLine, camera);
    float min_dist = minmax.x;
    float max_dist = minmax.y;

    auto world_to_physical = vol.getWorldToPhysicalMatrix();
    auto bounds = tgt::Bounds(vol.getLLF(), vol.getURB());
    tgt::vec2 zoomRegion = current.getZoomRegion();

    auto project3D = [&] (const PolyLine<tgt::vec2>& projectionLine) {
        std::vector<tgt::vec3> segment;
        for(int i=0; i<NUM_SAMPLES; ++i) {
            float projection_d = static_cast<float>(i)/(NUM_SAMPLES-1);

            tgt::vec2 projectionPoint = projectionLine.interpolate(projection_d);

            float proj_y = projectionPoint.y;
            // Undo zoom
            float normalized_depth = zoomRegion.y * proj_y + (1.0 - proj_y) * zoomRegion.x;

            float depth = normalized_depth * (max_dist - min_dist) + min_dist;

            float display_d = projectionPoint.x;
            tgtAssert(-0.1 < display_d && display_d < 1.1, "Invalid interpolation value"); // might happen due to numerical inaccuracies.
            display_d = tgt::clamp(display_d, 0.0f, 1.0f);

            tgt::vec2 display_point = displayLine.interpolate(display_d);

            tgt::vec3 normalized_query(display_point, 0.0);
            tgt::vec4 front_pos = front.getVoxelLinear(normalized_query * tgt::vec3(front.getDimensions()));
            tgt::vec4 back_pos = back.getVoxelLinear(normalized_query * tgt::vec3(front.getDimensions()));

            if(front_pos.a == 0.0 || back_pos.a == 0) {
                continue;
            }

            tgt::vec4 front_world = tex_to_world * front_pos;
            tgt::vec4 back_world = tex_to_world * back_pos;

            tgt::vec3 view_dir = tgt::normalize(back_world.xyz() - front_world.xyz());

            tgt::vec3 point = camera + depth*view_dir;

            // Perform clipping at volume boundary
            if(!bounds.containsPoint(world_to_physical.transform(point))) {
                continue;
            }
            segment.push_back(point);
        }
        return segment;
    };
    // Results
    current.foregroundLabels_.clear();
    current.backgroundLabels_.clear();

    for(auto& line : current.projectionLabels_.foreground_) {
        current.foregroundLabels_.push_back(project3D(line));
    }
    for(auto& line : current.projectionLabels_.background_) {
        current.backgroundLabels_.push_back(project3D(line));
    }
    seedsChanged_ = true;
}
InteractiveProjectionLabeling::InteractiveProjectionLabeling()
    : RenderProcessor()
    , inport_(Port::INPORT, "interactiveprojectionlabeling.inport", "Volume Input")
    , foregroundLabelGeometry_(Port::OUTPORT, "interactiveprojectionlabeling.foregroundLabelGeometry", "Foreground Labels Output")
    , backgroundLabelGeometry_(Port::OUTPORT, "interactiveprojectionlabeling.backgroundLabelGeometry", "Background Labels Output")
    , overlayOutput_(Port::OUTPORT, "interactiveprojectionlabeling.overlayoutput", "Overlay (3D)", true, Processor::INVALID_RESULT, RenderPort::RENDERSIZE_RECEIVER)
    , projectionOutput_(Port::OUTPORT, "interactiveprojectionlabeling.projectionoutput", "Projection (2D)", true, Processor::INVALID_RESULT, RenderPort::RENDERSIZE_RECEIVER)
    , fhp_(Port::INPORT, "interactiveprojectionlabeling.fhp", "First hit points", false)
    , lhp_(Port::INPORT, "interactiveprojectionlabeling.lhp", "Last hit points", false)
    , camera_("camera", "Camera")
    , projectionTransfunc_("transferFunction", "Projection Transfer Function")
    , initializationMode_("initializationMode", "Initialization Mode")
    , maxLineSimplificationDistance_("maxLineSimplificationDistance_", "Maximum Line Simplification Distance", 0.01, 0.0, 1.0)
    , backgroundLineDistanceMultiplier_("backgroundLineDistanceMultiplier", "Background Label Distance Multiplier", 1.00, 0.01, 10.0)
    , clearLabels_("clearLabels", "Clear All Labels")
    , projectionShader_("shader", "Shader", "interactiveprojectionlabeling.frag", "oit_passthrough.vert")
    , projection_(boost::none)
    , projectionLabelsModified_(false)
    , projectionRequiresUpdate_(false)
    , currentUnit_()
    , prevProjectionMousePos_(boost::none)
    , state_(FREE)
    , seedsChanged_(true)
    , labelUnits_()
    , currentUnitIndex_("currentUnitIndex", "Current Unit Index", -1, -1, INT_MAX)
    , clippingRegion_("clippingRegion", "Clip Region (Link with OptimizedProxyGeometry)", tgt::ivec3(0), tgt::ivec3(0), tgt::ivec3(INT_MAX))
{
    addPort(inport_);
    ON_CHANGE_LAMBDA(inport_, [this] () {
        seedsChanged_ = true;
    });
    addPort(foregroundLabelGeometry_);
    addPort(backgroundLabelGeometry_);
    addPort(overlayOutput_);
    addPort(projectionOutput_);
    addPort(fhp_);
    addPort(lhp_);

    overlayOutput_.onSizeReceiveChange<InteractiveProjectionLabeling>(this, &InteractiveProjectionLabeling::updateSizes);

    addProperty(projectionShader_);
    addProperty(camera_);
    addProperty(initializationMode_);
        initializationMode_.addOption("none", "None", NONE);
        initializationMode_.addOption("brightlumen", "Bright Lumen", BRIGHT_LUMEN);
        initializationMode_.addOption("brightwall", "Bright Wall", BRIGHT_WALL);
        ON_CHANGE(initializationMode_, InteractiveProjectionLabeling, initializeProjectionLabels);
    addProperty(projectionTransfunc_);
    addProperty(maxLineSimplificationDistance_);
        ON_CHANGE(maxLineSimplificationDistance_, InteractiveProjectionLabeling, initializeProjectionLabels);
    addProperty(backgroundLineDistanceMultiplier_);
        ON_CHANGE(backgroundLineDistanceMultiplier_, InteractiveProjectionLabeling, initializeProjectionLabels);

    addProperty(currentUnitIndex_);
        ON_CHANGE(currentUnitIndex_, InteractiveProjectionLabeling, synchronizeUnitIndex);

    addProperty(clippingRegion_);
        clippingRegion_.setReadOnlyFlag(true);

    addProperty(clearLabels_);
        ON_CHANGE_LAMBDA(clearLabels_, [this] () {
            resetCurrentUnit();
            currentUnitIndex_.setMaxValue(0);
            labelUnits_.clear();
            seedsChanged_ = true;
        });

}

void InteractiveProjectionLabeling::synchronizeUnitIndex() {
    auto index = currentUnitIndex();
    if(index) {
        currentUnit() = labelUnits_.at(*index);
        camera_.set(currentUnit().camera_);
        clippingRegion_.set(currentUnit().clippingRegion_);
        state_ = LABELING;
    } else {
        resetCurrentUnit();
        state_ = FREE;
    }
    projectionLabelsModified_ = true;
    projectionRequiresUpdate_ = true;
    invalidate();
}

void InteractiveProjectionLabeling::updateSizes() {
    projectionRequiresUpdate_ = true;
    invalidate();
}


void InteractiveProjectionLabeling::initialize() {
    RenderProcessor::initialize();
    currentUnitIndex_.setMaxValue(labelUnits_.size()-1);
}

void InteractiveProjectionLabeling::deinitialize() {
    RenderProcessor::deinitialize();
}
static void renderLine(const std::deque<tgt::vec2>& points, tgt::vec3 color) {
    MatStack.matrixMode(tgt::MatrixStack::PROJECTION);
    MatStack.pushMatrix();
    MatStack.ortho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);

    IMode.color(color);
    IMode.begin(tgt::ImmediateMode::LINE_STRIP);
    for(auto& p : points) {
        IMode.vertex(p);
    }
    IMode.end();

    glPointSize(5.0);
    IMode.begin(tgt::ImmediateMode::POINTS);
    for(auto& p : points) {
        IMode.vertex(p);
    }
    IMode.end();

    MatStack.popMatrix();

    MatStack.matrixMode(tgt::MatrixStack::MODELVIEW);
    IMode.color(tgt::vec4(1.0));
    glPointSize(1.0);
}
void InteractiveProjectionLabeling::renderOverlay() {
    overlayOutput_.activateTarget();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderLine(currentUnit().displayLine_, tgt::vec3(1.0, 0.0, 0.0));

    overlayOutput_.deactivateTarget();
}
void InteractiveProjectionLabeling::renderProjection() {
    if(!inport_.hasData()) {
        return;
    }
    const auto& vol = *inport_.getData();

    projectionTransfunc_.setVolume(inport_.getData(), 0);
    auto program = projectionShader_.getShader();
    if(!program || !program->isLinked()) {
        LGL_ERROR;
        LERROR("Shader not compiled!");
        return;
    }

    projectionOutput_.activateTarget();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if(projection_) {
        auto& p = *projection_;

        tgt::TextureUnit unit;
        unit.activate();
        p.bindTexture();

        tgt::TextureUnit transferUnit1;
        transferUnit1.activate();
        projectionTransfunc_.get()->getTexture()->bind();
        LGL_ERROR;

        program->activate();

        projectionTransfunc_.get()->setUniform(program, "transFuncParams_", "transFuncTex_", transferUnit1.getUnitNumber());
        program->setUniform("tex_", unit.getUnitNumber());

        auto rwm = vol.getRealWorldMapping();
        program->setUniform("rwmOffset_", rwm.getOffset());
        program->setUniform("rwmScale_", rwm.getScale());

        glDepthFunc(GL_ALWAYS);
        renderQuad();
        glDepthFunc(GL_LESS);

        program->deactivate();
        glActiveTexture(GL_TEXTURE0);
    }

    for(auto& line : currentUnit().projectionLabels_.foreground_) {
        renderLine(line, tgt::vec3(1.0, 0.0, 0.0));
    }
    for(auto& line : currentUnit().projectionLabels_.background_) {
        renderLine(line, tgt::vec3(0.0, 1.0, 0.0));
    }

    projectionOutput_.deactivateTarget();
    LGL_ERROR;
}

boost::optional<VolumeAtomic<tgt::vec4>> InteractiveProjectionLabeling::getFhp() {
    if(!tgt::isInitedGL() || !fhp_.getColorTexture()) {
        return boost::none;
    }
    return VolumeAtomic<tgt::vec4>((tgt::vec4*)fhp_.getColorTexture()->downloadTextureToBuffer(GL_RGBA, GL_FLOAT), tgt::svec3(fhp_.getSize(),1));
}
boost::optional<VolumeAtomic<tgt::vec4>> InteractiveProjectionLabeling::getLhp() {
    if(!tgt::isInitedGL() || !lhp_.getColorTexture()) {
        return boost::none;
    }
    return VolumeAtomic<tgt::vec4>((tgt::vec4*)lhp_.getColorTexture()->downloadTextureToBuffer(GL_RGBA, GL_FLOAT), tgt::svec3(lhp_.getSize(),1));
}

const int IPL_MAX_NEIGHBOR_OFFSET = 1;
const float IPL_BACKGROUND_DIST_MULTIPLIER = 1.5f;

static std::vector<int> maxPath(const VolumeAtomic<float>& img) {
    VolumeAtomic<int> paths(img.getDimensions());
    tgt::ivec3 idim = img.getDimensions();
    std::vector<float> global_cost(idim.x, 0.0);
    for(int y=0; y < idim.y; ++y) {
        std::vector<float> next_global_cost(idim.x, 0.0);
        for(int x=0; x < idim.x; ++x) {
            int best_i = 0;
            float best_val = 0.0;
            for(int d=std::max(0, x-IPL_MAX_NEIGHBOR_OFFSET); d<std::min(idim.x, x+IPL_MAX_NEIGHBOR_OFFSET+1); ++d) {
                float val = img.voxel(d, y, 0) + global_cost.at(d);
                if(val > best_val || (val == best_val && std::abs(best_i - x) > std::abs(d - x))) {
                    best_val = val;
                    best_i = d;
                }
            }
            paths.voxel(x, y, 0) = best_i;
            next_global_cost[x] = best_val;
        }
        global_cost = next_global_cost;
    }

    std::vector<int> path;
    float best_begin_val = 0.0;
    int best_begin_i = 0;
    for(int x=0; x < idim.x; ++x) {
        if(global_cost[x] > best_begin_val) {
            best_begin_val = global_cost[x];
            best_begin_i = x;
        }
    }
    int x = best_begin_i;
    path.push_back(x);
    for(int y=idim.y-2; y >= 0; --y) {
        x = paths.voxel(x, y, 0);
        path.push_back(x);
    }
    std::reverse(path.begin(), path.end());
    return path;
}
void simplifyPathInternal(std::deque<tgt::vec2>::const_iterator begin, std::deque<tgt::vec2>::const_iterator end, float max_line_dist, std::deque<tgt::vec2>& output) {
    if(std::distance(begin, end) <= 2) {
        if(begin != end) {
            output.push_back(*begin);
        }
        return;
    }

    auto& first = *begin;
    auto& last = *(end-1);
    Line line(first, last);
    boost::optional<std::deque<tgt::vec2>::const_iterator> farthest = boost::none;
    float max_dist = max_line_dist;
    for(auto it = begin+1; it != end-1; ++it) {
        float dist = line.dist(*it);
        if(dist > max_dist) {
            farthest = it;
            max_dist = dist;
        }
    }
    if(farthest) {
        simplifyPathInternal(begin, *farthest, max_line_dist, output);
        simplifyPathInternal(*farthest, end, max_line_dist, output);
    } else {
        output.push_back(*begin);
    }
}
void simplifyPath(std::deque<tgt::vec2>& input, float max_line_dist) {
    std::deque<tgt::vec2> output;

    simplifyPathInternal(input.cbegin(), input.cend(), max_line_dist, output);

    if(input.size() >= 2) {
        output.push_back(input.back());
    }

    input = output;
}

static void addLabelsFromWalls(ProjectionLabels& labels, const std::vector<int> lower_wall, const std::vector<int> upper_wall, float max_line_dist, tgt::ivec2 img_dim, float background_line_distance_multiplier) {

    tgtAssert(lower_wall.size() == upper_wall.size(), "Path size mismatch");

    std::deque<tgt::vec2> foreground;
    std::deque<tgt::vec2> upperBackground;
    std::deque<tgt::vec2> lowerBackground;

    for(int x=0; x < upper_wall.size(); ++x) {
        float x_pos = static_cast<float>(x)/(img_dim.x-1);
        float y_top = static_cast<float>(upper_wall.at(x))/(img_dim.y-1);
        float y_bottom = static_cast<float>(lower_wall.at(x))/(img_dim.y-1);

        float center = tgt::clamp((y_top + y_bottom)/2, 0.0f, 1.0f);
        float top_dist = y_top - center;
        float bottom_dist = y_bottom - center;
        foreground.emplace_back(x_pos, center);
        lowerBackground.emplace_back(x_pos, tgt::clamp(center+top_dist * background_line_distance_multiplier, 0.0f, 1.0f));
        upperBackground.emplace_back(x_pos, tgt::clamp(center+bottom_dist * background_line_distance_multiplier, 0.0f, 1.0f));

        // Actual detected borders (useful for debugging)
        //lowerBackground.emplace_back(x_pos, tgt::clamp(y_bottom, 0.0f, 1.0f));
        //upperBackground.emplace_back(x_pos, tgt::clamp(y_top, 0.0f, 1.0f));
    }

    simplifyPath(foreground, max_line_dist);
    simplifyPath(lowerBackground, max_line_dist);
    simplifyPath(upperBackground, max_line_dist);

    labels.foreground_.push_back(foreground);
    labels.background_.push_back(lowerBackground);
    labels.background_.push_back(upperBackground);
}

static void initBrightWall(const LabelProjection& proj, ProjectionLabels& labels, float max_line_dist, float background_line_distance_multiplier) {
    const auto& source = proj.projection();
    tgt::ivec3 idim = source.getDimensions();
    VolumeAtomic<float> original(tgt::svec3(idim.y, idim.x, idim.z));

    // Idea:
    // - find first possible run (possible wall) first
    // - clear around first run
    // - find second run
    //
    // In order to avoid crossing of runs:
    // - divide at center of initial lines
    // - find first wall in upper half
    // - find second wall in lower half
    // - find inner as center of both

    float sum = 0.0;

    //Transpose for path search
    for(int y=0; y < idim.y; ++y) {
        for(int x=0; x < idim.x; ++x) {
            tgt::vec2 p = source.voxel(x, y, 0);

            float val;
            if(p.y > 0.0) {
                val = p.x;
            } else {
                val = 0.0;
            }
            sum += val;
            original.voxel(y, x, 0) = val;
        }
    }
    float mean = sum / (idim.x * idim.y);

    auto first_path = maxPath(original);

    // Clear values around first path in order to find second maximum path
    VolumeAtomic<float> masked(original.copy());
    for(int x=0; x < first_path.size(); ++x) {
        int y = first_path[x];
        masked.voxel(y, x, 0) = mean;
        for(int wy = y+1; y < idim.y; ++wy) {
            float& val = masked.voxel(wy, x, 0);
            if(val > mean) {
                val = mean;
            } else {
                break;
            }
        }
        for(int wy = y-1; y >= 0; --wy) {
            float& val = masked.voxel(wy, x, 0);
            if(val > mean) {
                val = mean;
            } else {
                break;
            }
        }
    }
    auto second_path = maxPath(masked);

    // Create to new images with below/above center path masked
    VolumeAtomic<float> upper_masked(original.copy());
    VolumeAtomic<float> lower_masked(original.copy());
    for(int x=0; x < first_path.size(); ++x) {
        int center = (first_path[x] + second_path[x])/2;

        int y = 0;
        for(; y < center; ++y) {
            upper_masked.voxel(y, x, 0) = mean;
        }
        for(; y < idim.y; ++y) {
            lower_masked.voxel(y, x, 0) = mean;
        }
    }

    // Create upper/lower paths from masks
    auto upper_path = maxPath(lower_masked);
    auto lower_path = maxPath(upper_masked);

    addLabelsFromWalls(labels, upper_path, lower_path, max_line_dist, idim.xy(), background_line_distance_multiplier);
}

static inline std::vector<float> gaussFirstDerivativeKernel(float stddev, int extent) {
    auto func =  [stddev] (float x) {
        return -std::exp(-0.5f * x*x / (stddev*stddev))*x/(std::sqrt(2*tgt::PIf)*stddev*stddev*stddev);
    };
    auto gauss = [] (float stdev) {
        return [stdev] (float x) {
            return std::exp(-0.5f * x*x / (stdev*stdev))/(std::sqrt(2*tgt::PIf)*stdev);
        };
    };
    std::vector<float> kernel(2*extent + 1);
    float positiveSum = 0;
    float negativeSum = 0;
    for(int i = -extent; i <= extent; ++i) {
        int radius = 5;
        float sum = 0.0f;
        for(int d = -radius; d <= radius; ++d) {
            float df = static_cast<float>(d)/radius;
            float samplePos = static_cast<float>(i)-0.5f*df;
            sum += func(samplePos);
        }
        float val = sum/(2*radius + 1);
        if(val > 0) {
            positiveSum += val;
        } else {
            negativeSum += val;
        }
        kernel[i + extent] = val;
    }
    float positiveNormalizationFactor = std::abs(gauss(stddev)(0)/positiveSum);
    float negativeNormalizationFactor = std::abs(gauss(stddev)(0)/negativeSum);
    // Normalize so that all values sum to 0
    for(int i = -extent; i <= extent; ++i) {
        if(kernel[i + extent] > 0) {
            kernel[i + extent] *= positiveNormalizationFactor;
        } else {
            kernel[i + extent] *= negativeNormalizationFactor;
        }
    }
    return kernel;
}

static void initBrightLumen(const LabelProjection& proj, ProjectionLabels& labels, float max_line_dist, float background_line_distance_multiplier) {
    const auto& orig = proj.projection();
    tgt::ivec3 idim = orig.getDimensions();
    VolumeAtomic<float> transposed(tgt::svec3(idim.y, idim.x, idim.z));
    VolumeAtomic<float> top_gradients(tgt::svec3(idim.y, idim.x, idim.z));
    VolumeAtomic<float> bottom_gradients(tgt::svec3(idim.y, idim.x, idim.z));
    top_gradients.clear();
    bottom_gradients.clear();

    // Transpose matrix for better memory locality and fill invalid values with nearest valid ones
    int valid = 0;
    for(int x=0; x < idim.x; ++x) {
        float first = std::nan("");
        float last = 0.0;
        for(int y=0; y < idim.y; ++y) {
            tgt::vec2 val = orig.voxel(x, y, 0);
            if(val.y > 0.0) {
                if(std::isnan(first)) {
                    first = val.x;
                }
                last = val.x;
            }
        }
        bool before_first_valid = true;
        for(int y=0; y < idim.y; ++y) {
            tgt::vec2 val = orig.voxel(x, y, 0);
            if(val.y > 0.0) {
                transposed.voxel(y, x, 0) = val.x;
                before_first_valid = false;
            } else {
                if(before_first_valid) {
                    transposed.voxel(y, x, 0) = first;
                } else {
                    transposed.voxel(y, x, 0) = last;
                }
            }
        }
    }

    float std_dev_max = idim.y/10;
    std::vector<std::vector<float>> kernels;
    for(int stddev = 1; stddev < std_dev_max; stddev = stddev << 1) {
        int extent = 2*stddev;
        kernels.push_back(gaussFirstDerivativeKernel(stddev, extent));
    }

    auto center_path = maxPath(transposed);

#ifdef VRN_MODULE_OPENMP
#pragma omp parallel for
#endif
    for(int x=0; x < idim.x; ++x) {
        for(auto& kernel : kernels) {
            int extent = kernel.size() / 2; //kernel always has size 2extent+1
            for(int y=0; y < idim.y; ++y) {

                float sum = 0.0f;
                int extent_begin = std::max(y-extent,0) - y;
                int extent_end = std::min(y+extent,idim.y) - y;

                float* transposed_row_begin = &transposed.voxel(y+extent_begin, x, 0);
                int len = extent_end - extent_begin;

#ifdef VRN_MODULE_OPENMP
#pragma omp simd
#endif
                for(int i=0; i<len; ++i) {
                    float val = transposed_row_begin[i];
                    sum += val * kernel[i];
                }

                float& top = top_gradients.voxel(y, x, 0);
                float& bottom = bottom_gradients.voxel(y, x, 0);

                // mask below/above center line for top/bottom gradients
                int center = center_path[x];
                if(y < center) {
                    top = std::max(top, -sum);
                } else {
                    bottom = std::max(bottom, sum);
                }
            }
        }
    }

    auto lower_path = maxPath(bottom_gradients);
    auto upper_path = maxPath(top_gradients);

    addLabelsFromWalls(labels, upper_path, lower_path, max_line_dist, idim.xy(), background_line_distance_multiplier);
}

void InteractiveProjectionLabeling::initializeProjectionLabels() {
    currentUnit().projectionLabels_.clear();

    if(!projection_) {
        return;
    }

    switch(initializationMode_.getValue()) {
        case BRIGHT_LUMEN:
            {
                initBrightLumen(*projection_, currentUnit().projectionLabels_, maxLineSimplificationDistance_.get(), backgroundLineDistanceMultiplier_.get());
                break;
            }
        case BRIGHT_WALL:
            {
                initBrightWall(*projection_, currentUnit().projectionLabels_, maxLineSimplificationDistance_.get(), backgroundLineDistanceMultiplier_.get());
                break;
            }
        default:;
    }
    projectionLabelsModified_ = false;
}


void InteractiveProjectionLabeling::updateProjection() {
    if(!inport_.hasData()) {
        return;
    }

    projectionRequiresUpdate_ = false;
    if(currentUnit().displayLine_.empty()) {
        return;
    }
    const auto& vol = *inport_.getData();

    auto dim = overlayOutput_.getReceivedSize();
    projection_ = LabelProjection(dim);
    auto proj = projection_->projection_mut();

    auto maybe_front = getFhp();
    auto maybe_back = getLhp();
    if(!maybe_front || !maybe_back) {
        return;
    }
    auto& front = *maybe_front;
    auto& back = *maybe_back;

    tgt::vec3 camera = camera_.get().getPosition();

    auto line = PolyLine<tgt::vec2>(currentUnit().displayLine_);
    auto tex_to_world = vol.getTextureToWorldMatrix();

    auto minmax = projectionDepthRange(vol, front, back, line, camera);
    float min_dist = minmax.x;
    float max_dist = minmax.y;

    auto world_to_vox = vol.getWorldToVoxelMatrix();
    auto tex_to_vox = vol.getTextureToVoxelMatrix();

    tgt::vec3 max_dim = vol.getDimensions() - tgt::svec3::one;

    int y_block_size = 32;

    tgt::vec2 zoomRegion = currentUnit().getZoomRegion();

    std::function<float(tgt::vec3)> sample;
    if(vol.hasRepresentation<VolumeRAM>() || !vol.hasRepresentation<VolumeOctree>()) {
        const auto volram = vol.getRepresentation<VolumeRAM>();
        sample = [volram] (tgt::vec3 p) {
            return volram->getVoxelNormalizedLinear(p);
        };



#ifdef VRN_MODULE_OPENMP
#pragma omp parallel for
#endif
        for(int x = 0; x < dim.x; ++x) {

            float d = ((float)x)/(dim.x-1);
            auto p = line.interpolate(d);

            tgt::vec3 normalized_query(p, 0);
            tgt::vec4 front_pos = front.getVoxelLinear(normalized_query * tgt::vec3(front.getDimensions()));
            tgt::vec4 back_pos = back.getVoxelLinear(normalized_query * tgt::vec3(back.getDimensions()));

            tgt::vec4 front_world = tex_to_world * front_pos;
            tgt::vec4 back_world = tex_to_world * back_pos;

            tgt::vec3 view_dir = tgt::normalize(back_world.xyz() - front_world.xyz());

            for(int y = 0; y < dim.y; ++y) {
                float base_alpha = ((float)y)/(dim.y-1);
                float alpha = zoomRegion.y * base_alpha + (1.0 - base_alpha) * zoomRegion.x;
                float alpha_rw = max_dist * alpha + (1.0 - alpha) * min_dist;

                tgt::vec4 query_pos_rw(view_dir * alpha_rw + camera, 1.0);
                tgt::vec3 query_pos = (world_to_vox * query_pos_rw).xyz();

                tgt::vec2 val;
                if(tgt::hor(tgt::greaterThan(query_pos, max_dim)) || tgt::hor(tgt::lessThan(query_pos, tgt::vec3::zero))
                        || back_world.a == 0.0 || front_pos.a == 0.0) {
                    val = tgt::vec2(0.0, 0.0);
                } else {
                    val = tgt::vec2(sample(query_pos), 1.0);
                }

                proj.at(tgt::svec2(x, y)) = val;
            }
        }

    } else {
        const auto octree = vol.getRepresentation<VolumeOctree>();

        LocatedVolumeOctreeNodeConst root = octree->getLocatedRootNode();

        float base_dist_within_volume = max_dist - min_dist;
        float dist_within_volume = base_dist_within_volume * (zoomRegion.y - zoomRegion.x);

        if(!std::isfinite(dist_within_volume)) {
            dist_within_volume = tgt::length(vol.getCubeSize());
        }
        float voxels_in_projection_depth = tgt::min(world_to_vox.transform(tgt::vec3(dist_within_volume)));

        float voxels_along_line = 0.0f;
        for(int i=0; i<((int)line.points_.size())-1; ++i) {
            auto& p1 = line.points_[i];
            auto& p2 = line.points_[i+1];

            auto to_vox = [&] (tgt::vec2 p) {
                tgt::vec4 front_pos = front.getVoxelLinear(tgt::vec3(p, 0.0f) * tgt::vec3(front.getDimensions()));
                return tex_to_vox * front_pos;
            };

            voxels_along_line += tgt::distance(to_vox(p1.pos_), to_vox(p2.pos_));
        }

        float level_y = std::log2(voxels_in_projection_depth / static_cast<float>(dim.y));
        float level_x = std::log2(voxels_along_line / static_cast<float>(dim.x));
        size_t level = tgt::clamp(std::floor(std::min(level_x, level_y)), 0.0f, static_cast<float>(octree->getNumLevels()-1));

        // Estimate pixel block size for projection to cover roughly one leaf node of octree
        // For VolumeRAM it doesn't really matter.
        int voxels_per_block = tgt::hadd(octree->getBrickDim())/3; //roughly
        float blocks_in_projection_depth = voxels_in_projection_depth / std::max(1, voxels_per_block);
        y_block_size = tgt::clamp(static_cast<int>(std::round(static_cast<float>(dim.y) / blocks_in_projection_depth)), 1, dim.y);
        tgtAssert(y_block_size >= 0, "invalid block size");

        const tgt::svec3 brickDataSize = octree->getBrickDim();

        struct BrickCacheEntry {
            LocatedVolumeOctreeNodeConst node;
            tgt::mat4 voxelToBrick;
            BrickPoolBrickConst brick;

            BrickCacheEntry(LocatedVolumeOctreeNodeConst node, tgt::svec3 brickDataSize, const OctreeBrickPoolManagerBase& brickPoolManager)
                : node(node)
                , voxelToBrick(node.location().voxelToBrick())
                , brick(node.node().getBrickAddress(), brickDataSize, brickPoolManager)
            {
            }
        };

        const size_t BRICK_CACHE_SIZE = 8;

#ifdef VRN_MODULE_OPENMP
#pragma omp parallel for
#endif
        for(int y_base = 0; y_base < dim.y; y_base+=y_block_size) {
            int y_max = std::min(dim.y, y_base + y_block_size);

            std::deque<BrickCacheEntry> brickCache;

            for(int x = 0; x < dim.x; ++x) {

                float d = ((float)x)/(dim.x-1);
                auto p = line.interpolate(d);

                tgt::vec3 normalized_query(p, 0);
                tgt::vec4 front_pos = front.getVoxelLinear(normalized_query * tgt::vec3(front.getDimensions()));
                tgt::vec4 back_pos = back.getVoxelLinear(normalized_query * tgt::vec3(back.getDimensions()));

                tgt::vec4 front_world = tex_to_world * front_pos;
                tgt::vec4 back_world = tex_to_world * back_pos;

                tgt::vec3 view_dir = tgt::normalize(back_world.xyz() - front_world.xyz());

                for(int y = y_base; y < y_max; ++y) {
                    float base_alpha = ((float)y)/(dim.y-1);
                    float alpha = zoomRegion.y * base_alpha + (1.0 - base_alpha) * zoomRegion.x;
                    float alpha_rw = max_dist * alpha + (1.0 - alpha) * min_dist;

                    tgt::vec4 query_pos_rw(view_dir * alpha_rw + camera, 1.0);
                    tgt::vec3 query_pos = (world_to_vox * query_pos_rw).xyz();

                    tgt::vec2 val;
                    if(tgt::hor(tgt::greaterThan(query_pos, max_dim)) || tgt::hor(tgt::lessThan(query_pos, tgt::vec3::zero))
                            || back_world.a == 0.0 || front_pos.a == 0.0) {
                        val = tgt::vec2(0.0, 0.0);
                    } else {
                        tgt::svec3 pVoxel = tgt::round(query_pos);
                        BrickCacheEntry* cacheEntry = nullptr;
                        for(auto it = brickCache.begin(); it != brickCache.end(); ++it) {
                            auto prevLlf = it->node.location().voxelLLF();
                            auto prevUrb = it->node.location().voxelURB();
                            if(tgt::hand(tgt::lessThanEqual(prevLlf, pVoxel) & tgt::lessThan(pVoxel, prevUrb))) {
                                cacheEntry = &*it;
                                break;
                            }
                        }
                        if(cacheEntry == nullptr) {
                            brickCache.emplace_back(root.findChildNode(pVoxel, brickDataSize, level, false), brickDataSize, *octree->getBrickPoolManager());
                            if(brickCache.size() > BRICK_CACHE_SIZE) {
                                brickCache.pop_front();
                            }
                            cacheEntry = &brickCache.back();
                        }
                        tgt::svec3 pos = tgt::round(cacheEntry->voxelToBrick.transform(query_pos));
                        float voxel = cacheEntry->brick.getVoxelNormalized(pos);

                        val = tgt::vec2(voxel, 1.0);
                    }

                    proj.at(tgt::svec2(x, y)) = val;
                }
            }
        }
    }

    if(!projectionLabelsModified_) {
        initializeProjectionLabels();
    }
}

InteractiveProjectionLabeling::~InteractiveProjectionLabeling() {
}

void InteractiveProjectionLabeling::process() {
    if (getInvalidationLevel() == INVALID_PROGRAM) {
        std::string header;
        header += projectionTransfunc_.get()->getShaderDefines();
        projectionShader_.setHeader(header);
        projectionShader_.rebuild();
    }

    if(projectionRequiresUpdate_) {
        updateProjection();
    }

    renderOverlay();
    renderProjection();

    if(seedsChanged_) {

        std::unique_ptr<PointSegmentListGeometryVec3> foregroundLabelLines(new PointSegmentListGeometryVec3());
        std::unique_ptr<PointSegmentListGeometryVec3> backgroundLabelLines(new PointSegmentListGeometryVec3());

        for(auto& unit : labelUnits_) {
            for(auto& l : unit.foregroundLabels_) {
                foregroundLabelLines->addSegment(l);
            }
            for(auto& l : unit.backgroundLabels_) {
                backgroundLabelLines->addSegment(l);
            }
        }

        foregroundLabelGeometry_.setData(foregroundLabelLines.release());
        backgroundLabelGeometry_.setData(backgroundLabelLines.release());
        seedsChanged_ = false;
    }
}


void InteractiveProjectionLabeling::adjustPropertiesToInput() {
    if(!inport_.hasData()) {
        return;
    }
    projectionTransfunc_.setVolume(inport_.getData(), 0);

    projectionRequiresUpdate_ = true;
}
VoreenSerializableObject* InteractiveProjectionLabeling::create() const {
    return new InteractiveProjectionLabeling();
}

LabelUnit::LabelUnit()
    : camera_()
    , clippingRegion_()
    , displayLine_()
    , projectionLabels_()
    , zoomRegion_(0.0f, 1.0f)
    , foregroundLabels_()
    , backgroundLabels_()
{
}

void LabelUnit::setZoomRegion(tgt::vec2 newRegion) {
    tgtAssert(newRegion.x < newRegion.y, "Invalid zoom region");
    auto applyZoom = [&] (float& p) {
        float normalized = zoomRegion_.x + p * (zoomRegion_.y - zoomRegion_.x);
        p = (normalized - newRegion.x) / (newRegion.y - newRegion.x);
    };

    for(auto& line : projectionLabels_.foreground_) {
        for(auto& p : line) {
            applyZoom(p.y);
        }
    }
    for(auto& line : projectionLabels_.background_) {
        for(auto& p : line) {
            applyZoom(p.y);
        }
    }
    zoomRegion_ = newRegion;
}
tgt::vec2 LabelUnit::getZoomRegion() const {
    return zoomRegion_;
}
void LabelUnit::serialize(Serializer& s) const {
    s.serialize("camera_position", camera_.getPosition());
    s.serialize("camera_focus", camera_.getFocus());
    s.serialize("camera_upVector", camera_.getUpVector());
    s.serialize("camera_frustLeft", camera_.getFrustLeft());
    s.serialize("camera_frustRight", camera_.getFrustRight());
    s.serialize("camera_frustBottom", camera_.getFrustBottom());
    s.serialize("camera_frustTop", camera_.getFrustTop());
    s.serialize("camera_frustNear", camera_.getNearDist(false));
    s.serialize("camera_frustFar", camera_.getFarDist(false));
    s.serialize("camera_useOrthoZoomFactor", camera_.getUseOrthoZoomFactorFlag());
    s.serialize("camera_orthoZoomFactor", camera_.getOrthoZoomFactor());
    s.serialize("camera_useRealWorldFrustum", camera_.getUseRealWorldFrustum());

    s.serialize("clippingRegion", clippingRegion_);
    s.serialize("displayLine", displayLine_);

    s.serialize("zoomRegion", zoomRegion_);

    s.serialize("foregroundLabels2D", projectionLabels_.foreground_);
    s.serialize("backgroundLabels2D", projectionLabels_.background_);

    s.serialize("foregroundLabels3D", foregroundLabels_);
    s.serialize("backgroundLabels3D", backgroundLabels_);
}

void LabelUnit::deserialize(Deserializer& s) {
    {
        float left, right, bottom, top, nearP, farP;
        s.deserialize("camera_frustLeft", left);
        s.deserialize("camera_frustRight", right);
        s.deserialize("camera_frustBottom", bottom);
        s.deserialize("camera_frustTop", top);
        s.deserialize("camera_frustNear", nearP);
        s.deserialize("camera_frustFar", farP);
        camera_.setFrustum(tgt::Frustum(left, right, bottom, top, nearP, farP));
    }
    {
        tgt::vec3 position, focus, upVector;
        s.deserialize("camera_position", position);
        s.deserialize("camera_focus", focus);
        s.deserialize("camera_upVector", upVector);

        camera_.setPosition(position);
        camera_.setFocus(focus);
        camera_.setUpVector(upVector);
    }
    {
        float orthoZoomFactor;
        bool useOrthoZoomFactor, useRealWorldFrustum;
        s.deserialize("camera_useOrthoZoomFactor", useOrthoZoomFactor);
        s.deserialize("camera_orthoZoomFactor", orthoZoomFactor);
        s.deserialize("camera_useRealWorldFrustum", useRealWorldFrustum);

        camera_.setUseOrthoZoomFactorFlag(useOrthoZoomFactor);
        camera_.setOrthoZoomFactor(orthoZoomFactor);
        camera_.setUseRealWorldFrustum(useRealWorldFrustum);
    }

    s.deserialize("clippingRegion", clippingRegion_);
    s.deserialize("displayLine", displayLine_);

    try {
        s.deserialize("zoomRegion", zoomRegion_);
    } catch(SerializationException& e) {
        s.removeLastError();
        zoomRegion_ = tgt::vec2(0.0f, 1.0f);
    }
    s.deserialize("foregroundLabels2D", projectionLabels_.foreground_);
    s.deserialize("backgroundLabels2D", projectionLabels_.background_);

    s.deserialize("foregroundLabels3D", foregroundLabels_);
    s.deserialize("backgroundLabels3D", backgroundLabels_);
}

void InteractiveProjectionLabeling::serialize(Serializer& s) const {
    Processor::serialize(s);

    s.serialize("labelUnits", labelUnits_);
}

void InteractiveProjectionLabeling::deserialize(Deserializer& s) {
    s.deserialize("labelUnits", labelUnits_);

    Processor::deserialize(s);
}

LabelUnit& InteractiveProjectionLabeling::currentUnit() {
    return currentUnit_;
}

void InteractiveProjectionLabeling::resetCurrentUnit() {
    currentUnit() = LabelUnit();
    currentUnitIndex_.set(-1);
    projection_ = boost::none;
}

boost::optional<size_t> InteractiveProjectionLabeling::currentUnitIndex() {
    auto index = currentUnitIndex_.get();
    if(index != -1) {
        return index;
    } else {
        return boost::none;
    }
}

} // namespace voreen
