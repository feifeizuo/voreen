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

#ifndef VRN_DYNAMICPYTHONPROCESSOR_H
#define VRN_DYNAMICPYTHONPROCESSOR_H

#include "voreen/core/processors/renderprocessor.h"

#include "voreen/core/ports/volumeport.h"

#include "modules/base/properties/interactivelistproperty.h"

#include "../properties/pythonproperty.h"

namespace voreen {

class VRN_CORE_API DynamicPythonProcessor : public RenderProcessor {
public:
    DynamicPythonProcessor();
    ~DynamicPythonProcessor();

    virtual Processor* create() const;

    virtual std::string getClassName() const  { return "DynamicPythonProcessor"; }
    virtual std::string getCategory() const   { return "Python";                 }
    virtual CodeState getCodeState() const    { return CODE_STATE_EXPERIMENTAL;  }

    virtual bool isReady() const;

    PythonProperty* getPythonProperty() {
        return &pythonProperty_;
    }

protected:

    virtual void setDescriptions() {
        setDescription("This processor allows to make use of a python script to process incoming data. ");
        portList_.setDescription("Add and remove ports (using drag and drop) to be accessible from within the python script");
    }

    virtual void process();
    virtual void initialize();
    virtual void deinitialize();

    virtual void serialize(Serializer& s) const;
    virtual void deserialize(Deserializer& s);

private:

    void onPortListChange();
    void onScriptChange();

    void addPortItem(Port* port);

    std::vector<std::unique_ptr<Port>> portItems_;

    InteractiveListProperty portList_;
    BoolProperty enabled_;
    PythonProperty pythonProperty_;
    PythonScript pythonScript_;
    ButtonProperty runScriptButton_;

    bool valid_;

    static const std::string loggerCat_;
};

} // namespace

#endif // VRN_DYNAMICGLSLPROCESSOR_H
