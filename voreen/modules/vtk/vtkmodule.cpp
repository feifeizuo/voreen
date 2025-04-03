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

#include "vtkmodule.h"

#include <vtkSmartPointer.h>
#include <vtkOutputWindow.h>

#include "io/netcdfvolumereader.h"
#include "io/niftivolumewriter.h"
#include "io/vtivolumereader.h"
#include "io/vtivolumewriter.h"
#include "io/vtmvolumereader.h"

namespace voreen {

class VtkOutputRedirect : public vtkOutputWindow
{
public:
    vtkTypeMacro(VtkOutputRedirect, vtkOutputWindow);
    static VtkOutputRedirect* New()
    {
        return new VtkOutputRedirect;
    }

    void DisplayText(const char* text) override
    {
        LINFOC("VTK", text);
    }

    void DisplayErrorText(const char* text) override
    {
        LERRORC("VTK", text);
    }

    void DisplayWarningText(const char* text) override
    {
        LWARNINGC("VTK", text);
    }

    void DisplayGenericWarningText(const char* text) override
    {
        LDEBUGC("VTK", text);
    }
};

VTKModule::VTKModule(const std::string& modulePath)
    : VoreenModule(modulePath)
{
    setID("VTK");
    setGuiName("VTK");

    registerVolumeReader(new NetCDFVolumeReader());
    registerVolumeReader(new VTIVolumeReader());
    registerVolumeReader(new VTMVolumeReader());
    registerVolumeWriter(new NiftiVolumeWriter());
    registerVolumeWriter(new VTIVolumeWriter());

    // On windows, the VTK library might log warnings in an annoying window popping up.
    // We redirect the output to our own logging mechanism (for consistency on every OS).
    vtkOutputWindow::SetInstance(vtkSmartPointer<VtkOutputRedirect>::New());
}

} // namespace
