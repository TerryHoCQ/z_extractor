/*****************************************************************************
*                                                                            *
*  Copyright (C) 2023 Liu An Lin <liuanlin-mx@qq.com>                        *
*                                                                            *
*  Licensed under the Apache License, Version 2.0 (the "License");           *
*  you may not use this file except in compliance with the License.          *
*  You may obtain a copy of the License at                                   *
*                                                                            *
*      http://www.apache.org/licenses/LICENSE-2.0                            *
*                                                                            *
*  Unless required by applicable law or agreed to in writing, software       *
*  distributed under the License is distributed on an "AS IS" BASIS,         *
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  *
*  See the License for the specific language governing permissions and       *
*  limitations under the License.                                            *
*                                                                            *
*****************************************************************************/

#include <complex>
#include "calc.h"
#include "openems_model_gen.h"
float openems_model_gen::C0 = 299792458;
openems_model_gen::openems_model_gen(const std::shared_ptr<pcb>& pcb)
    : _pcb(pcb)
    , _mesh_x_min_gap(0.1)
    , _mesh_y_min_gap(0.1)
    , _mesh_z_min_gap(0.01)
    , _lambda_mesh_ratio(20)
    , _pix_unit(0.05)
    , _ignore_cu_thickness(true)
    , _bc(BC_PML)
    , _f0(0)
    , _fc(3e9)
    , _far_field_freq(2.4e9)
{
    _pcb->ignore_cu_thickness(_ignore_cu_thickness);
}

openems_model_gen::~openems_model_gen()
{
}

void openems_model_gen::add_net(std::uint32_t net_id, bool gen_mesh, bool zone_gen_mesh, std::uint32_t mesh_prio)
{
    mesh_info info;
    info.gen_mesh = gen_mesh;
    info.zone_gen_mesh = zone_gen_mesh;
    info.mesh_prio = mesh_prio;
    _nets.insert(std::pair<std::uint32_t, mesh_info>(net_id, info));
}

void openems_model_gen::add_net(std::uint32_t net_id, float x_gap, float y_gap, bool zone_gen_mesh, std::uint32_t mesh_prio)
{
    mesh_info info;
    info.gen_mesh = true;
    info.use_uniform_grid = true;
    info.x_gap = x_gap;
    info.y_gap = y_gap;
    info.zone_gen_mesh = zone_gen_mesh;
    info.mesh_prio = mesh_prio;
    _nets.insert(std::pair<std::uint32_t, mesh_info>(net_id, info));
}

void openems_model_gen::add_footprint(const std::string & footprint, bool gen_mesh, std::uint32_t mesh_prio)
{
    mesh_info info;
    info.gen_mesh = gen_mesh;
    info.mesh_prio = mesh_prio;
    _footprints.insert(std::pair<std::string, mesh_info>(footprint, info));
}

void openems_model_gen::add_footprint(const std::string& footprint, float x_gap, float y_gap, std::uint32_t mesh_prio)
{
    mesh_info info;
    info.gen_mesh = true;
    info.use_uniform_grid = true;
    info.x_gap = x_gap;
    info.y_gap = y_gap;
    info.mesh_prio = mesh_prio;
    _footprints.insert(std::pair<std::string, mesh_info>(footprint, info));
}

void openems_model_gen::add_excitation(const std::string& fp1, const std::string& fp1_pad_number, const std::string& fp1_layer_name,
                        const std::string& fp2, const std::string& fp2_pad_number, const std::string& fp2_layer_name, std::uint32_t dir, float R, bool gen_mesh)
{
    return add_lumped_port(fp1, fp1_pad_number, fp1_layer_name,
                        fp2, fp2_pad_number, fp2_layer_name,
                        dir, R, true, gen_mesh);
}

void openems_model_gen::add_excitation(pcb::point start, const std::string& start_layer, pcb::point end, const std::string& end_layer, std::uint32_t dir, float R, bool gen_mesh)
{
    excitation ex;
    ex.gen_mesh = gen_mesh;
    ex.R = R;
    ex.dir = dir;
    ex.start.x = start.x;
    ex.start.y = start.y;
    ex.start.z = _pcb->get_layer_z_axis(start_layer);
    ex.end.x = end.x;
    ex.end.y = end.y;
    ex.end.z = _pcb->get_layer_z_axis(end_layer);
    _excitations.push_back(ex);
}

void openems_model_gen::add_lumped_port(const std::string& fp1, const std::string& fp1_pad_number, const std::string& fp1_layer_name,
                        const std::string& fp2, const std::string& fp2_pad_number, const std::string& fp2_layer_name,
                        std::uint32_t dir, float R, bool excite, bool gen_mesh)
{
    pcb::pad pad1;
    pcb::pad pad2;
    pcb::footprint footprint1;
    pcb::footprint footprint2;
    if (_pcb->get_footprint(fp1, footprint1) && _pcb->get_footprint(fp2, footprint2)
            && _pcb->get_pad(fp1, fp1_pad_number, pad1)
            && _pcb->get_pad(fp2, fp2_pad_number, pad2))
    {
        pcb::point p1 = pad1.at;
        pcb::point p2 = pad2.at;
        _pcb->get_rotation_pos(footprint1.at, footprint1.at_angle, p1);
        _pcb->get_rotation_pos(footprint2.at, footprint2.at_angle, p2);
        
    
        excitation ex;
        ex.gen_mesh = gen_mesh;
        ex.R = R;
        ex.dir = dir;
        ex.excite = excite;
        
        ex.start.z = _pcb->get_layer_z_axis(fp1_layer_name);
        ex.end.z = _pcb->get_layer_z_axis(fp2_layer_name);
        
        if (dir == excitation::DIR_X)
        {
            ex.start.x = _round_xy(p1.x);
            ex.end.x = _round_xy(p2.x);
            
            if (gen_mesh)
            {
                ex.start.y = _round_xy(p1.y);
                ex.end.y = _round_xy(p2.y);
            }
            else
            {
                float size = std::min(std::min(pad1.size_w, pad1.size_h), std::min(pad2.size_w, pad2.size_h)) / 2;
                ex.start.y = p1.y - size;
                ex.end.y = p1.y + size;
            }
        }
        else if (dir == excitation::DIR_Y)
        {
            if (gen_mesh)
            {
                ex.start.x = _round_xy(p1.x);
                ex.end.x = _round_xy(p2.x);
            }
            else
            {
                float size = std::min(std::min(pad1.size_w, pad1.size_h), std::min(pad2.size_w, pad2.size_h)) / 2;
                ex.start.x = p1.x - size;
                ex.end.x = p2.x + size;
            }
            
            ex.start.y = _round_xy(p1.y);
            ex.end.y = _round_xy(p2.y);
        }
        else if (dir == excitation::DIR_Z)
        {
            if (gen_mesh)
            {
                ex.start.x = _round_xy(p1.x);
                ex.start.y = _round_xy(p1.y);
                ex.end.x = _round_xy(p2.x);
                ex.end.y = _round_xy(p2.y);
            }
            else
            {
                float size = std::min(std::min(pad1.size_w, pad1.size_h), std::min(pad2.size_w, pad2.size_h)) / 2;
                
                ex.start.x = p1.x - size;
                ex.start.y = p1.y - size;
                
                ex.end.x = p1.x + size;
                ex.end.y = p1.y + size;
            }
        }
        _excitations.push_back(ex);
    }
    else
    {
        printf("add_lumped_port err\n");
    }
}
                        
void openems_model_gen::add_lumped_port(const std::string& fp_name, bool excite, bool gen_mesh)
{
    std::string ref = fp_name;
    pcb::footprint footprint;
    
    if (ref.front() != 'R' && ref.front() != 'r')
    {
        printf("add_lumped_port: err %s\n", ref.c_str());
        return;
    }
    
    if (_pcb->get_footprint(fp_name, footprint) && footprint.pads.size() == 2)
    {
        pcb::point p1 = footprint.pads[0].at;
        pcb::point p2 = footprint.pads[1].at;
        const pcb::pad& pad1 = footprint.pads[0];
        const pcb::pad& pad2 = footprint.pads[1];
        _pcb->get_rotation_pos(footprint.at, footprint.at_angle, p1);
        _pcb->get_rotation_pos(footprint.at, footprint.at_angle, p2);
        
    
        std::uint32_t dir = lumped_element::DIR_X;
        
        if (fabs(p1.x - p2.x) > fabs(p1.y - p2.y))
        {
            dir = lumped_element::DIR_X;
        }
        else
        {
            dir = lumped_element::DIR_Y;
        }
        
        excitation ex;
        ex.gen_mesh = gen_mesh;
        ex.R = _string_to_float(footprint.value);
        ex.dir = dir;
        ex.excite = excite;
        
        ex.start.z = _pcb->get_layer_z_axis(footprint.layer);
        ex.end.z = ex.start.z;
        
        if (dir == lumped_element::DIR_X)
        {
            ex.start.x = _round_xy(p1.x);
            ex.end.x = _round_xy(p2.x);
            
            if (gen_mesh)
            {
                ex.start.y = _round_xy(p1.y);
                ex.end.y = _round_xy(p2.y);
            }
            else
            {
                float size = std::min(std::min(pad1.size_w, pad1.size_h), std::min(pad2.size_w, pad2.size_h)) / 2;
                ex.start.y = p1.y - size;
                ex.end.y = p1.y + size;
            }
        }
        else if (dir == lumped_element::DIR_Y)
        {
            if (gen_mesh)
            {
                ex.start.x = _round_xy(p1.x);
                ex.end.x = _round_xy(p2.x);
            }
            else
            {
                float size = std::min(std::min(pad1.size_w, pad1.size_h), std::min(pad2.size_w, pad2.size_h)) / 2;
                ex.start.x = p1.x - size;
                ex.end.x = p1.x + size;
            }
            
            ex.start.y = _round_xy(p1.y);
            ex.end.y = _round_xy(p2.y);
        }
        _excitations.push_back(ex);
    }
    else
    {
        printf("add_lumped_port: err %s\n", ref.c_str());
    }
}

void openems_model_gen::add_lumped_element(const std::string& fp1, const std::string& fp1_pad_number, const std::string& fp1_layer_name,
                        const std::string& fp2, const std::string& fp2_pad_number, const std::string& fp2_layer_name,
                        std::uint32_t dir, std::uint32_t type, float v, bool gen_mesh)
{
    
    pcb::pad pad1;
    pcb::pad pad2;
    pcb::footprint footprint1;
    pcb::footprint footprint2;
    if (_pcb->get_footprint(fp1, footprint1) && _pcb->get_footprint(fp2, footprint2)
            && _pcb->get_pad(fp1, fp1_pad_number, pad1)
            && _pcb->get_pad(fp2, fp2_pad_number, pad2))
    {
        pcb::point p1 = pad1.at;
        pcb::point p2 = pad2.at;
        _pcb->get_rotation_pos(footprint1.at, footprint1.at_angle, p1);
        _pcb->get_rotation_pos(footprint2.at, footprint2.at_angle, p2);
        
    
        lumped_element element;
        element.name = "LE_" + fp1 + fp1_pad_number + fp1_layer_name + "_" + fp2 + fp2_pad_number + fp2_layer_name;
        element.gen_mesh = gen_mesh;
        element.type = type;
        element.v = v;
        element.dir = dir;
        
        element.start.z = _pcb->get_layer_z_axis(fp1_layer_name);
        element.end.z = _pcb->get_layer_z_axis(fp2_layer_name);
        
        if (dir == lumped_element::DIR_X)
        {
            element.start.x = _round_xy(p1.x);
            element.end.x = _round_xy(p2.x);
            
            if (gen_mesh)
            {
                element.start.y = _round_xy(p1.y);
                element.end.y = _round_xy(p2.y);
            }
            else
            {
                float size = std::min(std::min(pad1.size_w, pad1.size_h), std::min(pad2.size_w, pad2.size_h)) / 2;
                element.start.y = p1.y - size;
                element.end.y = p1.y + size;
            }
        }
        else if (dir == lumped_element::DIR_Y)
        {
            if (gen_mesh)
            {
                element.start.x = _round_xy(p1.x);
                element.end.x = _round_xy(p2.x);
            }
            else
            {
                float size = std::min(std::min(pad1.size_w, pad1.size_h), std::min(pad2.size_w, pad2.size_h)) / 2;
                element.start.x = p1.x - size;
                element.end.x = p1.x + size;
            }
            
            element.start.y = _round_xy(p1.y);
            element.end.y = _round_xy(p2.y);
        }
        else if (dir == lumped_element::DIR_Z)
        {
            if (gen_mesh)
            {
                element.start.x = _round_xy(p1.x);
                element.start.y = _round_xy(p1.y);
                element.end.x = _round_xy(p2.x);
                element.end.y = _round_xy(p2.y);
            }
            else
            {
                float size = std::min(std::min(pad1.size_w, pad1.size_h), std::min(pad2.size_w, pad2.size_h)) / 2;
                element.start.x = p1.x - size;
                element.start.y = p1.y - size;
                
                element.end.x = p1.x + size;
                element.end.y = p1.y + size;
            }
        }
        _lumped_elements.push_back(element);
    }
    else
    {
        printf("add_lumped_element err\n");
    }
}

void openems_model_gen::add_lumped_element(const std::string& fp_name, bool gen_mesh)
{
    std::string ref = fp_name;
    pcb::footprint footprint;
    std::uint32_t type = lumped_element::TYPE_R;
    if (ref.front() == 'R' || ref.front() == 'r')
    {
        type = lumped_element::TYPE_R;
    }
    else if (ref.front() == 'L' || ref.front() == 'l')
    {
        type = lumped_element::TYPE_L;
    }
    else if (ref.front() == 'C' || ref.front() == 'c')
    {
        type = lumped_element::TYPE_C;
    }
    else
    {
        return;
    }
    
    if (_pcb->get_footprint(fp_name, footprint) && footprint.pads.size() == 2)
    {
        pcb::point p1 = footprint.pads[0].at;
        pcb::point p2 = footprint.pads[1].at;
        const pcb::pad& pad1 = footprint.pads[0];
        const pcb::pad& pad2 = footprint.pads[1];
        _pcb->get_rotation_pos(footprint.at, footprint.at_angle, p1);
        _pcb->get_rotation_pos(footprint.at, footprint.at_angle, p2);
        
    
        std::uint32_t dir = lumped_element::DIR_X;
        
        if (fabs(p1.x - p2.x) > fabs(p1.y - p2.y))
        {
            dir = lumped_element::DIR_X;
        }
        else
        {
            dir = lumped_element::DIR_Y;
        }
        
        lumped_element element;
        element.name = "LE_" + fp_name;
        element.gen_mesh = gen_mesh;
        element.type = type;
        element.v = _string_to_float(footprint.value);
        element.dir = dir;
        
        element.start.z = _pcb->get_layer_z_axis(footprint.layer);
        element.end.z = element.start.z;
        
        if (dir == lumped_element::DIR_X)
        {
            element.start.x = _round_xy(p1.x);
            element.end.x = _round_xy(p2.x);
            
            if (gen_mesh)
            {
                element.start.y = _round_xy(p1.y);
                element.end.y = _round_xy(p2.y);
            }
            else
            {
                float size = std::min(std::min(pad1.size_w, pad1.size_h), std::min(pad2.size_w, pad2.size_h)) / 2;
                element.start.y = p1.y - size;
                element.end.y = p1.y + size;
            }
        }
        else if (dir == lumped_element::DIR_Y)
        {
            if (gen_mesh)
            {
                element.start.x = _round_xy(p1.x);
                element.end.x = _round_xy(p2.x);
            }
            else
            {
                float size = std::min(std::min(pad1.size_w, pad1.size_h), std::min(pad2.size_w, pad2.size_h)) / 2;
                element.start.x = p1.x - size;
                element.end.x = p1.x + size;
            }
            
            element.start.y = _round_xy(p1.y);
            element.end.y = _round_xy(p2.y);
        }
        _lumped_elements.push_back(element);
    }
    else
    {
        printf("add_lumped_element err\n");
    }
}
                        
void openems_model_gen::add_lumped_element(pcb::point start, const std::string& start_layer, pcb::point end, const std::string& end_layer, std::uint32_t dir, std::uint32_t type, float v, bool gen_mesh)
{
    lumped_element element;
    char buf[256];
    sprintf(buf, "LE_%s_x%.2f_y%.2f_%s_x%.2f_y%.2f", start_layer.c_str(), start.x, start.y, end_layer.c_str(), end.x, end.y);
    element.name = buf;
    element.gen_mesh = gen_mesh;
    element.type = type;
    element.v = v;
    element.dir = dir;
    element.start.x = start.x;
    element.start.y = start.y;
    element.start.z = _pcb->get_layer_z_axis(start_layer);
    element.end.x = end.x;
    element.end.y = end.y;
    element.end.z = _pcb->get_layer_z_axis(end_layer);
    _lumped_elements.push_back(element);
}



void openems_model_gen::add_freq(float freq)
{
    _freq.push_back(freq);
}

void openems_model_gen::add_mesh_range(float start, float end, float gap, std::uint32_t dir, std::uint32_t prio)
{
    mesh::line_range range(start, end, gap, prio);
    if (dir == mesh::DIR_X)
    {
        _mesh.x_range.insert(range);
    }
    else if (dir == mesh::DIR_Y)
    {
        _mesh.y_range.insert(range);
    }
    else if (dir == mesh::DIR_Z)
    {
        _mesh.z_range.insert(range);
    }
}


void openems_model_gen::set_boundary_cond(std::uint32_t bc)
{
    _bc = bc;
}

void openems_model_gen::set_nf2ff_footprint(const std::string& fp)
{
    _nf2ff_fp = fp;
}

void openems_model_gen::set_excitation_freq(float f0, float fc)
{
    _f0 = f0;
    _fc = fc;
}

void openems_model_gen::set_far_field_freq(float freq)
{
    _far_field_freq = freq;
}

void openems_model_gen::set_mesh_min_gap(float x_min_gap, float y_min_gap, float z_min_gap)
{
    _mesh_x_min_gap = x_min_gap;
    _mesh_y_min_gap = y_min_gap;
    _mesh_z_min_gap = z_min_gap;
}

void openems_model_gen::gen_model(const std::string& func_name)
{
    FILE *fp = fopen((func_name + ".m").c_str(), "wb");
    if (fp)
    {
        fprintf(fp, "function [CSX] = %s(CSX, max_freq)\n", func_name.c_str());
        fprintf(fp, "physical_constants;\n");
        fprintf(fp, "unit = 1e-3;\n");
        
        //_gen_mesh_z(fp);
        _add_dielectric(fp);
        _add_metal(fp);
        _add_segment(fp);
        _add_via(fp);
        _add_zone(fp);
        _add_footprint(fp);
        //_gen_mesh_xy(fp);
        fprintf(fp, "end\n");
        fclose(fp);
    }
}


void openems_model_gen::gen_mesh(const std::string& func_name)
{
    FILE *fp = fopen((func_name + ".m").c_str(), "wb");
    if (fp)
    {
        fprintf(fp, "function [CSX, mesh] = %s(CSX, max_freq)\n", func_name.c_str());
        fprintf(fp, "physical_constants;\n");
        fprintf(fp, "unit = 1e-3;\n");
        
        _apply_mesh_line_range(_mesh);
        
        _gen_mesh_z(fp);
        _gen_mesh_xy(fp);
        fprintf(fp, "end\n\n\n");
        fclose(fp);
    }
}


void openems_model_gen::gen_antenna_simulation_scripts()
{
    FILE *fp = fopen("antenna_simulation_scripts.m", "wb");
    if (fp)
    {
        fprintf(fp, "close all; clear; clc;\n");
        fprintf(fp, "show_model = 1;\n");
        fprintf(fp, "plot_only = 0;\n");
        fprintf(fp, "physical_constants;\n");
        fprintf(fp, "unit = 1e-3;\n");
        fprintf(fp, "max_timesteps = 1e9; min_decrement = 1e-5;\n");
        fprintf(fp, "FDTD = InitFDTD('NrTS', max_timesteps, 'EndCriteria', min_decrement);\n");
        fprintf(fp, "f0 = %e; fc = %e;\n", _f0, _fc);
        //fprintf(fp, "lambda = c0 / (f0 + fc) / unit;\n");
        fprintf(fp, "FDTD = SetGaussExcite(FDTD, f0, fc);\n");
        if (_bc == BC_PML)
        {
            fprintf(fp, "BC = {'PML_8' 'PML_8' 'PML_8' 'PML_8' 'PML_8' 'PML_8'};\n");
        }
        else
        {
            fprintf(fp, "BC = {'MUR' 'MUR' 'MUR' 'MUR' 'MUR' 'MUR'};\n");
        }
        fprintf(fp, "FDTD = SetBoundaryCond(FDTD, BC);\n");
        fprintf(fp, "\n");
        fprintf(fp, "CSX = InitCSX();\n");
        
        
        _add_lumped_element(fp, 99);
        _add_excitation(fp, 99);
        _add_nf2ff_box(fp);
        fprintf(fp, "sim_path = 'ant_sim'; plot_path = 'plot'; sim_csx = 'ant.xml';\n");
        fprintf(fp, "if (plot_only == 0)\n");
        fprintf(fp, "    CSX = load_pcb_model(CSX, f0 + fc);\n");
        fprintf(fp, "    [CSX, mesh] = load_pcb_mesh(CSX, f0 + fc);\n");
        fprintf(fp, "    CSX = DefineRectGrid(CSX, unit, mesh);\n");
        fprintf(fp, "\n");
        fprintf(fp, "    rmdir(sim_path, 's');\n");
        fprintf(fp, "    mkdir(sim_path);\n");
        fprintf(fp, "    mkdir(plot_path);\n");
        fprintf(fp, "    WriteOpenEMS([sim_path '/' sim_csx], FDTD, CSX);\n");
        fprintf(fp, "    if (show_model == 1)\n");
        fprintf(fp, "        CSXGeomPlot([sim_path '/' sim_csx], ['--export-STL=' sim_path]);\n");
        fprintf(fp, "    end\n");
        fprintf(fp, "    RunOpenEMS(sim_path, sim_csx, '--debug-PEC');\n");
        fprintf(fp, "end\n");
        
        fprintf(fp, "printf('\\n\\n');\n");
        _add_read_ui(fp);
        _add_plot_s11(fp);
        _add_plot_vswr(fp);
        _add_plot_feed_point_impedance(fp);
        
        _add_plot_far_field(fp);

        //fprintf(fp, "%s\n", plot_s);
        fprintf(fp, "\n");
        fprintf(fp, "\n");
        //fprintf(fp, "%s\n", plot_nf);
        
        fclose(fp);
    }
    gen_model("load_pcb_model");
    gen_mesh("load_pcb_mesh");
}

void openems_model_gen::gen_two_port_sparamer_scripts()
{
    
    FILE *fp = fopen("two_port_sparamer.m", "wb");
    if (fp)
    {
        fprintf(fp, "close all; clear; clc;\n");
        fprintf(fp, "show_model = 1;\n");
        fprintf(fp, "plot_only = 0;\n");
        fprintf(fp, "physical_constants;\n");
        fprintf(fp, "unit = 1e-3;\n");
        fprintf(fp, "max_timesteps = 1e9; min_decrement = 1e-2;\n");
        fprintf(fp, "FDTD = InitFDTD('NrTS', max_timesteps, 'EndCriteria', min_decrement);\n");
        fprintf(fp, "f0 = %e; fc = %e;\n", _f0, _fc);
        fprintf(fp, "FDTD = SetGaussExcite(FDTD, f0, fc);\n");
        if (_bc == BC_PML)
        {
            fprintf(fp, "BC = {'PML_8' 'PML_8' 'PML_8' 'PML_8' 'PML_8' 'PML_8'};\n");
        }
        else
        {
            fprintf(fp, "BC = {'MUR' 'MUR' 'MUR' 'MUR' 'MUR' 'MUR'};\n");
        }
        fprintf(fp, "FDTD = SetBoundaryCond(FDTD, BC);\n");
        fprintf(fp, "\n");
        
        fprintf(fp, "CSX = InitCSX();\n");
        
        
        
        _add_lumped_element(fp, 99);
        _add_excitation(fp, 99);
        
        fprintf(fp, "sim_path = 'two_sparamer'; plot_path = 'plot'; sim_csx = 'two_sparamer.xml';\n");
        fprintf(fp, "if (plot_only == 0)\n");
        fprintf(fp, "    CSX = load_pcb_model(CSX, f0 + fc);\n");
        fprintf(fp, "    [CSX, mesh] = load_pcb_mesh(CSX, f0 + fc);\n");
        fprintf(fp, "    CSX = DefineRectGrid(CSX, unit, mesh);\n");
        fprintf(fp, "\n");
        fprintf(fp, "    rmdir(sim_path, 's');\n");
        fprintf(fp, "    mkdir(sim_path);\n");
        fprintf(fp, "    mkdir(plot_path);\n");
        fprintf(fp, "    WriteOpenEMS([sim_path '/' sim_csx], FDTD, CSX);\n");
        fprintf(fp, "    if (show_model == 1)\n");
        fprintf(fp, "        CSXGeomPlot([sim_path '/' sim_csx], ['--export-STL=' sim_path]);\n");
        fprintf(fp, "    end\n");
        fprintf(fp, "    RunOpenEMS(sim_path, sim_csx, '--debug-PEC');\n");
        fprintf(fp, "end\n");
        
        fprintf(fp, "printf('\\n\\n');\n");
        _add_read_ui(fp);
        
        _add_plot_two_sparamer(fp);
        _add_plot_feed_point_impedance(fp);
        
        fprintf(fp, "\n");
        fprintf(fp, "\n");
        
        fclose(fp);
    }
    gen_model("load_pcb_model");
    gen_mesh("load_pcb_mesh");
}


void openems_model_gen::_gen_mesh_z(FILE *fp)
{
    std::string str;
    
    std::set<mesh::line> mesh_z = _mesh.z;
    _mesh.z.clear();
    
    float min_z = _pcb->get_cu_min_thickness();
    if (_ignore_cu_thickness)
    {
        min_z = _pcb->get_min_thickness(pcb::layer::DIELECTRIC);
    }
    
    std::vector<pcb::layer> layers = _pcb->get_layers();
    std::string last_layer;
    for (const auto& layer: layers)
    {
        if (layer.type == pcb::layer::TOP_SOLDER_MASK || layer.type == pcb::layer::BOTTOM_SOLDER_MASK)
        {
            continue;
        }
        if (_ignore_cu_thickness && layer.type == pcb::layer::COPPER)
        {
            continue;
        }
        float z = _pcb->get_layer_z_axis(layer.name);
        _mesh.z.insert(mesh::line(z, 0));
        last_layer = layer.name;
    }
    _mesh.z.insert(mesh::line(_pcb->get_layer_z_axis(last_layer) + _pcb->get_layer_thickness(last_layer), 0));
    
    _clean_mesh_line(_mesh.z, _mesh_z_min_gap);
    
    fprintf(fp, "mesh.z = [");
    for (auto z: _mesh.z)
    {
        fprintf(fp, "%f ", z.v);
    }
    fprintf(fp, "];\n");
    
    fprintf(fp, "max_res = %f;\n", min_z);
    fprintf(fp, "mesh.z = SmoothMeshLines(mesh.z, max_res, 1.3);\n");
    
    
    
    float lambda = C0 / (_f0 + _fc) * 1e3;
    float margin = std::max(lambda, _pcb->get_board_thickness() * 20);
    if (mesh_z.size() > 1)
    {
        fprintf(fp, "mesh.z = unique([mesh.z, %f, %f]);\n", std::min(-margin, mesh_z.begin()->v), std::max(margin, mesh_z.rbegin()->v));
    }
    else
    {
        fprintf(fp, "margin = %g;\n", margin);
        fprintf(fp, "mesh.z = unique([mesh.z, -margin, margin]);\n");
    }
    
    fprintf(fp, "max_res = c0 / (max_freq) / unit / %f;\n", _lambda_mesh_ratio);
    fprintf(fp, "mesh.z = SmoothMeshLines(mesh.z, max_res, 1.3);\n");
    
    fprintf(fp, "\n\n");
}


void openems_model_gen::_gen_mesh_xy(FILE *fp)
{
    float x1 = _pcb->get_edge_left();
    float x2 = _pcb->get_edge_right();
    float y1 = _pcb->get_edge_top();
    float y2 = _pcb->get_edge_bottom();
    
    float lambda = C0 / (_f0 + _fc) * 1e3;
    
    float ratio = (_bc == BC_PML)? _lambda_mesh_ratio / 10: _lambda_mesh_ratio / 4;
    
    float left = x1 - lambda / ratio;
    float right = x2 + lambda / ratio;
    float top = y1 - lambda / ratio;
    float bottom = y2 + lambda / ratio;
    
    if (!_mesh.x.empty() && _mesh.x.begin()->v > left)
    {
        _mesh.x.insert(mesh::line(left, _mesh.x.begin()->prio));
    }
    
    if (!_mesh.x.empty() && _mesh.x.rbegin()->v < right)
    {
        _mesh.x.insert(mesh::line(right, _mesh.x.rbegin()->prio));
    }
    
    if (!_mesh.y.empty() && _mesh.y.begin()->v > top)
    {
        _mesh.y.insert(mesh::line(top, _mesh.y.begin()->prio));
    }
    
    if (!_mesh.y.empty() && _mesh.y.rbegin()->v < bottom)
    {
        _mesh.y.insert(mesh::line(bottom, _mesh.y.rbegin()->prio));
    }
    
    
    
    
    _clean_mesh_line(_mesh.x, _mesh_x_min_gap);
    _clean_mesh_line(_mesh.y, _mesh_y_min_gap);
    
    fprintf(fp, "mesh.x = [");
    for (auto x: _mesh.x)
    {
        fprintf(fp, "%f ", x.v);
    }
    fprintf(fp, "];\n");
    
    fprintf(fp, "mesh.y = [");
    for (auto y: _mesh.y)
    {
        fprintf(fp, "%f ", y.v);
    }
    fprintf(fp, "];\n");
    
    
    fprintf(fp, "max_res = c0 / (max_freq) / unit / %f;\n", _lambda_mesh_ratio);
    fprintf(fp, "mesh.x = SmoothMeshLines(mesh.x, max_res, 1.3);\n");
    fprintf(fp, "mesh.y = SmoothMeshLines(mesh.y, max_res, 1.3);\n");
}


void openems_model_gen::_add_dielectric(FILE *fp)
{
    float x1 = _pcb->get_edge_left();
    float x2 = _pcb->get_edge_right();
    float y1 = _pcb->get_edge_top();
    float y2 = _pcb->get_edge_bottom();
    
    std::vector<pcb::layer> layers = _pcb->get_layers();
    for (const auto& layer: layers)
    {
        if (layer.type == pcb::layer::TOP_SOLDER_MASK
            || layer.type == pcb::layer::BOTTOM_SOLDER_MASK
            || layer.type == pcb::layer::COPPER)
        {
            continue;
        }
        
        float z1 = _pcb->get_layer_z_axis(layer.name);
        float z2 = z1 + _pcb->get_layer_thickness(layer.name);
        
        fprintf(fp, "start = [%f %f %f];\n", x1, y1, z1);
        fprintf(fp, "stop = [%f %f %f];\n", x2, y2, z2);
        fprintf(fp, "CSX = AddMaterial(CSX, '%s');\n", layer.name.c_str());
        fprintf(fp, "CSX = SetMaterialProperty(CSX, '%s', 'Epsilon', %f);\n", layer.name.c_str(), _pcb->get_layer_epsilon_r(layer.name));
        fprintf(fp, "CSX = AddBox(CSX, '%s', 1, start, stop);\n", layer.name.c_str());
    }
    fprintf(fp, "\n\n");
}


void openems_model_gen::_add_metal(FILE *fp)
{
    for (auto net: _nets)
    {
        std::uint32_t net_id = net.first;
        std::string net_name = _pcb->get_net_name(net_id);
        fprintf(fp, "CSX = AddMetal(CSX, '%s');\n", net_name.c_str());
    }
    fprintf(fp, "\n\n");
}


void openems_model_gen::_add_segment(FILE *fp)
{
    for (auto net: _nets)
    {
        std::uint32_t net_id = net.first;
        const mesh_info& info = net.second;
        std::list<pcb::segment> segments = _pcb->get_segments(net_id);
        
        float x_min = 100000;
        float x_max = -100000;
        float y_min = 100000;
        float y_max = -100000;
                
        for (const auto& s: segments)
        {
            const std::string& layer = s.layer_name;
            float z1 = _pcb->get_layer_z_axis(layer);
            float thickness = _pcb->get_layer_thickness(layer);
            if (s.is_arc())
            {
                double cx;
                double cy;
                double radius;
                calc_arc_center_radius(s.start.x, s.start.y, s.mid.x, s.mid.y, s.end.x, s.end.y, cx, cy, radius);
                
                
                float s_len = _pcb->get_segment_len(s);
                if (s_len < s.width * 0.5)
                {
                    continue;
                }
                
                std::list<pcb::point> points;
                for (float i = 0; i < s_len + 0.1; i += 0.1)
                {
                    float x = 0;
                    float y = 0;
                    _pcb->get_segment_pos(s, i, x, y);
                    
                    std::complex<float> c(cx, cy);
                    std::complex<float> p(x, y);
                    
                    std::complex<float> unit_vector = (p - c) / std::abs(p - c);
                    std::complex<float> p1 = p + unit_vector * (s.width / 2);
                    std::complex<float> p2 = p - unit_vector * (s.width / 2);
                    
                    points.push_back(pcb::point(p1.real(), p1.imag()));
                    points.push_front(pcb::point(p2.real(), p2.imag()));
                }
                std::uint32_t idx = 1;
                for (const auto& p: points)
                {
                    fprintf(fp, "p(1, %d) = %f; p(2, %d) = %f;\n", idx, p.x, idx, p.y);
                    idx++;
                }
                fprintf(fp, "CSX = AddLinPoly(CSX, '%s', 2, 2, %f, p, %f, 'CoordSystem', 0);\n", _pcb->get_net_name(s.net).c_str(), z1, thickness);
                fprintf(fp, "clear p;\n");
            }
            else
            {
                std::complex<float> start(s.start.x, s.start.y);
                std::complex<float> end(s.end.x, s.end.y);
                std::complex<float> unit_vector = 1;
                if (std::abs(end - start) < s.width * 0.5)
                {
                    continue;
                }
                
                unit_vector = (end - start) / std::abs(end - start);
                
                std::uint32_t idx = 1;
                
                float n = 4;
                for (std::int32_t i = 0; i <= n; i++)
                {
                    std::complex<float> tmp = std::polar(std::abs(unit_vector), (float)(std::arg(unit_vector) + M_PI_2 + i * M_PI / n));
                    std::complex<float> p = start + tmp * (s.width / 2);
                    fprintf(fp, "p(1, %d) = %f; p(2, %d) = %f;\n", idx, p.real(), idx, p.imag());
                    idx++;
                    if (info.gen_mesh && !info.use_uniform_grid)
                    {
                        _mesh.x.insert(mesh::line(p.real(), info.mesh_prio));
                        _mesh.y.insert(mesh::line(p.imag(), info.mesh_prio));
                    }
                }
                
                for (std::int32_t i = 0; i <= n; i++)
                {
                    std::complex<float> tmp = std::polar(std::abs(unit_vector), (float)(std::arg(unit_vector) + -M_PI_2 + i * M_PI / n));
                    std::complex<float> p = end + tmp * (s.width / 2);
                    fprintf(fp, "p(1, %d) = %f; p(2, %d) = %f;\n", idx, p.real(), idx, p.imag());
                    idx++;
                    if (info.gen_mesh && !info.use_uniform_grid)
                    {
                        _mesh.x.insert(mesh::line(p.real(), info.mesh_prio));
                        _mesh.y.insert(mesh::line(p.imag(), info.mesh_prio));
                    }
                }
                
                
                fprintf(fp, "CSX = AddLinPoly(CSX, '%s', 2, 2, %f, p, %f, 'CoordSystem', 0);\n", _pcb->get_net_name(s.net).c_str(), z1, thickness);
                fprintf(fp, "clear p;\n");
            }
            
            x_min = std::min(x_min, std::min(s.start.x - s.width, s.end.x - s.width));
            x_max = std::max(x_max, std::max(s.start.x + s.width, s.end.x + s.width));
            y_min = std::min(y_min, std::min(s.start.y - s.width, s.end.y - s.width));
            y_max = std::max(y_max, std::max(s.start.y + s.width, s.end.y + s.width));
        }
        if (info.gen_mesh && info.use_uniform_grid
            && x_min < 100000 && x_max > -100000 && x_min < x_max
            && y_min < 100000 && y_max > -100000 && y_min < y_max)
        {
            mesh::line_range x_range(x_min, x_max, info.x_gap, info.mesh_prio);
            mesh::line_range y_range(y_min, y_max, info.y_gap, info.mesh_prio);
            _mesh.x_range.insert(x_range);
            _mesh.y_range.insert(y_range);
        #if 0
            for (float x = x_min; x < x_max; x += info.x_gap)
            {
                _mesh.x.insert(mesh::line(x, info.mesh_prio));
            }
            for (float y = y_min; y < y_max; y += info.y_gap)
            {
                _mesh.y.insert(mesh::line(y, info.mesh_prio));
            }
        #endif
        }
    }
    fprintf(fp, "\n\n");
    fprintf(fp, "\n\n");
}


void openems_model_gen::_add_via(FILE *fp)
{
    for (auto net: _nets)
    {
        std::uint32_t net_id = net.first;
        const mesh_info& info = net.second;
        std::string net_name = _pcb->get_net_name(net_id);
        std::list<pcb::via> vias = _pcb->get_vias(net_id);
        for (const auto& v: vias)
        {
            std::vector<std::string> layers = _pcb->get_via_layers(v);
    
            float min_z = 10000;
            float max_z = -10000;
            for (const auto& layer: layers)
            {
                pcb::point c(v.at);
                float z1 = _pcb->get_layer_z_axis(layer);
                float thickness = _pcb->get_layer_thickness(layer);
                float z2 = z1 + thickness;
                
                if (z1 < min_z)
                {
                    min_z = z1;
                }
                
                if (z2 < min_z)
                {
                    min_z = z2;
                }
                
                if (z1 > max_z)
                {
                    max_z = z1;
                }
                
                if (z2 > max_z)
                {
                    max_z = z2;
                }
                
        #if 0
                float radius = v.size / 2;
                
                fprintf(fp, "CSX = AddCylinder(CSX, '%s', 2, [%f %f %f], [%f %f %f], %f);\n",
                        net_name.c_str(),
                        c.x, c.y, z1,
                        c.x, c.y, _ignore_cu_thickness? z2 + 0.001: z2,
                        radius);
        #endif
                        
            }
            if (min_z < 10000 && max_z > -10000)
            {
                pcb::point c(v.at);
                
                float radius = v.drill / 2;
                fprintf(fp, "CSX = AddCylinder(CSX, '%s', 2, [%f %f %f], [%f %f %f], %f);\n",
                            net_name.c_str(),
                            c.x, c.y, min_z,
                            c.x, c.y, max_z,
                            radius);
                if (info.gen_mesh && !info.use_uniform_grid)
                {
                    _mesh.x.insert(mesh::line(c.x, info.mesh_prio));
                    //_mesh_x.insert(c.x + radius);
                    //_mesh_x.insert(c.x - radius);
                    _mesh.y.insert(mesh::line(c.y, info.mesh_prio));
                    //_mesh_y.insert(c.y + radius);
                    //_mesh_y.insert(c.y - radius);
                }
            }
        }
    }
    fprintf(fp, "\n\n");
    fprintf(fp, "\n\n");
}


void openems_model_gen::_add_zone(FILE *fp)
{
    for (auto net: _nets)
    {
        std::uint32_t net_id = net.first;
        const mesh_info& info = net.second;
        std::list<pcb::zone> zones = _pcb->get_zones(net_id);
        for (const auto& z: zones)
        {
            const std::string& layer = z.layer_name;
            float z1 = _pcb->get_layer_z_axis(layer);
            float thickness = _pcb->get_layer_thickness(layer);
            
            std::uint32_t idx = 1;
            for (const auto& p: z.pts)
            {
                fprintf(fp, "p(1, %d) = %f; p(2, %d) = %f;\n", idx, p.x, idx, p.y);
                idx++;
                if (info.zone_gen_mesh)
                {
                    _mesh.x.insert(mesh::line(p.x, info.mesh_prio));
                    _mesh.y.insert(mesh::line(p.y, info.mesh_prio));
                }
            }
            
            fprintf(fp, "CSX = AddLinPoly(CSX, '%s', 2, 2, %f, p, %f, 'CoordSystem', 0);\n", _pcb->get_net_name(z.net).c_str(), z1, thickness);
            fprintf(fp, "clear p;\n");
        }
    }
    fprintf(fp, "\n\n");
    fprintf(fp, "\n\n");
}

void openems_model_gen::_add_footprint(FILE *fp)
{
    const std::vector<pcb::footprint>& footprints = _pcb->get_footprints();
    for (const auto& footprint: footprints)
    {
        if (_footprints.count(footprint.reference))
        {
            range_det range;
            const mesh_info& info = _footprints[footprint.reference];
            fprintf(fp, "CSX = AddMetal(CSX, '%s');\n", footprint.reference.c_str());
            for (const auto& gr: footprint.grs)
            {
                if (_pcb->is_cu_layer(gr.layer_name))
                {
                    _add_gr(gr, footprint.at, footprint.at_angle, footprint.reference, fp, range, info.mesh_prio, info.gen_mesh && !info.use_uniform_grid);
                }
            }
            
            for (const auto& pad: footprint.pads)
            {
                _add_pad(footprint, pad, footprint.reference, fp, range, info.mesh_prio, info.gen_mesh && !info.use_uniform_grid);
            }
            
            if (info.gen_mesh && info.use_uniform_grid && range.is_valid())
            {
                float x_margin = std::min(5.f, std::max(1.f, (range.x_max - range.x_min) / 20));
                float y_margin = std::min(5.f, std::max(1.f, (range.y_max - range.y_min) / 20));
                mesh::line_range x_range(range.x_min - x_margin, range.x_max + x_margin, info.x_gap, info.mesh_prio);
                mesh::line_range y_range(range.y_min - y_margin, range.y_max + y_margin, info.y_gap, info.mesh_prio);
                _mesh.x_range.insert(x_range);
                _mesh.y_range.insert(y_range);
            }
        }
    }
    fprintf(fp, "\n\n");
    fprintf(fp, "\n\n");
}


void openems_model_gen::_add_gr(const pcb::gr& gr, pcb::point at, float angle, const std::string& name, FILE *fp, range_det& range, std::uint32_t mesh_prio, bool gen_mesh)
{
    const std::string& layer = gr.layer_name;
    float z1 = _pcb->get_layer_z_axis(layer);
    float thickness = _pcb->get_layer_thickness(layer);
    float z2 = z1 + thickness;
    
    if (gr.gr_type == pcb::gr::GR_POLY)
    {
        std::uint32_t idx = 1;
        for (auto xy : gr.pts)
        {
            _pcb->get_rotation_pos(at, angle, xy);
            fprintf(fp, "p(1, %d) = %f; p(2, %d) = %f;\n", idx, xy.x, idx, xy.y);
            idx++;
            range.det(xy.x, xy.y);
            if (gen_mesh)
            {
                _mesh.x.insert(mesh::line(xy.x, mesh_prio));
                _mesh.y.insert(mesh::line(xy.y, mesh_prio));
            }
        }
        
        fprintf(fp, "CSX = AddLinPoly(CSX, '%s', 2, 2, %f, p, %f, 'CoordSystem', 0);\n", name.c_str(), z1, thickness);
        fprintf(fp, "clear p;\n");
    }
    else if (gr.gr_type == pcb::gr::GR_RECT)
    {
        pcb::point p1(gr.start.x, gr.start.y);
        pcb::point p2(gr.end.x, gr.start.y);
        pcb::point p3(gr.end.x, gr.end.y);
        pcb::point p4(gr.start.x, gr.end.y);
        
        _pcb->get_rotation_pos(at, angle, p1);
        _pcb->get_rotation_pos(at, angle, p2);
        _pcb->get_rotation_pos(at, angle, p3);
        _pcb->get_rotation_pos(at, angle, p4);
        
        if (gr.fill_type == pcb::gr::FILL_SOLID)
        {
            fprintf(fp, "p(1, 1) = %f; p(2, 1) = %f;\n", p1.x, p1.y);
            fprintf(fp, "p(1, 2) = %f; p(2, 2) = %f;\n", p2.x, p2.y);
            fprintf(fp, "p(1, 3) = %f; p(2, 3) = %f;\n", p3.x, p3.y);
            fprintf(fp, "p(1, 4) = %f; p(2, 4) = %f;\n", p4.x, p4.y);
        
            //fprintf(fp, "CSX = AddBox(CSX, '%s', 2, [%f %f %f], [%f %f %f]);\n",
            //    name.c_str(), p1.x, p1.y, z1, p3.x, p3.y, z2);
            fprintf(fp, "CSX = AddLinPoly(CSX, '%s', 2, 2, %f, p, %f, 'CoordSystem', 0);\n", name.c_str(), z1, thickness);
            fprintf(fp, "clear p;\n");
            
            range.det(p1.x, p1.y);
            range.det(p2.x, p2.y);
            range.det(p3.x, p3.y);
            range.det(p4.x, p4.y);
            if (gen_mesh)
            {
                _mesh.x.insert(mesh::line(p1.x, mesh_prio)); _mesh.y.insert(mesh::line(p1.y, mesh_prio));
                _mesh.x.insert(mesh::line(p2.x, mesh_prio)); _mesh.y.insert(mesh::line(p2.y, mesh_prio));
                _mesh.x.insert(mesh::line(p3.x, mesh_prio)); _mesh.y.insert(mesh::line(p3.y, mesh_prio));
                _mesh.x.insert(mesh::line(p4.x, mesh_prio)); _mesh.y.insert(mesh::line(p4.y, mesh_prio));
            }
        }
    }
    else if (gr.gr_type == pcb::gr::GR_LINE)
    {
        pcb::point start = gr.start;
        pcb::point end = gr.end;
        _pcb->get_rotation_pos(at, angle, start);
        _pcb->get_rotation_pos(at, angle, end);
        
        //cv::Point p1(_cvt_img_x(start.x, pix_unit), _cvt_img_y(start.y, pix_unit));
        //cv::Point p2(_cvt_img_x(end.x, pix_unit), _cvt_img_y(end.y, pix_unit));
        //float thickness = _cvt_img_len(gr.stroke_width, pix_unit);
        //cv::line(img, p1, p2, cv::Scalar(b, g, r), thickness);
    }
    else if (gr.gr_type == pcb::gr::GR_CIRCLE)
    {
        pcb::point start = gr.start;
        pcb::point end = gr.end;
        _pcb->get_rotation_pos(at, angle, start);
        _pcb->get_rotation_pos(at, angle, end);
        float radius = calc_dist(start.x, start.y, end.x, end.y);
        if (gr.fill_type == pcb::gr::FILL_SOLID)
        {
            fprintf(fp, "CSX = AddCylinder(CSX, '%s', 2, [%f %f %f], [%f %f %f], %f);\n",
                        name.c_str(),
                        start.x, start.y, z1,
                        start.x, start.y, z2,
                        radius);
            
            range.det(start.x, start.y);
            if (gen_mesh)
            {
                _mesh.x.insert(mesh::line(start.x, mesh_prio));
                _mesh.y.insert(mesh::line(start.y, mesh_prio));
            }
        }
        else
        {
            //float thickness = _cvt_img_len(gr.stroke_width, pix_unit);
        }
    }
    fprintf(fp, "\n");
}


void openems_model_gen::_add_pad(const pcb::footprint& footprint, const pcb::pad& p, const std::string& name, FILE *fp, range_det& range, std::uint32_t mesh_prio, bool gen_mesh)
{
    std::vector<std::string> layers = _pcb->get_pad_layers(p);
    
    if (p.type == pcb::pad::TYPE_THRU_HOLE)
    {
        float min_z = 10000;
        float max_z = -10000;
        
        for (const auto& layer: layers)
        {
            float z1 = _pcb->get_layer_z_axis(layer);
            float thickness = _pcb->get_layer_thickness(layer);
            float z2 = z1 + thickness;
            if (z1 < min_z)
            {
                min_z = z1;
            }
            
            if (z2 < min_z)
            {
                min_z = z2;
            }
            
            if (z1 > max_z)
            {
                max_z = z1;
            }
            
            if (z2 > max_z)
            {
                max_z = z2;
            }
        }
        
        if (min_z < 10000 && max_z > -10000)
        {
            pcb::point c(p.at);
            
            _pcb->get_rotation_pos(footprint.at, footprint.at_angle, c);
            float radius = p.drill / 2;
            fprintf(fp, "CSX = AddCylinder(CSX, '%s', 3, [%f %f %f], [%f %f %f], %f);\n",
                        name.c_str(),
                        c.x, c.y, min_z,
                        c.x, c.y, max_z,
                        radius);
            range.det(c.x, c.y);
            if (gen_mesh)
            {
                //_mesh_x.insert(c.x);
                _mesh.x.insert(mesh::line(c.x + radius, mesh_prio));
                _mesh.x.insert(mesh::line(c.x - radius, mesh_prio));
                //_mesh_y.insert(c.y);
                _mesh.y.insert(mesh::line(c.y + radius, mesh_prio));
                _mesh.y.insert(mesh::line(c.y - radius, mesh_prio));
            }
        }
    }
    
    for (const auto& layer: layers)
    {
        float z1 = _pcb->get_layer_z_axis(layer);
        float thickness = _pcb->get_layer_thickness(layer);
        float z2 = z1 + thickness;
        
        if (p.shape == pcb::pad::SHAPE_RECT || p.shape == pcb::pad::SHAPE_ROUNDRECT)
        {
            pcb::point p1(p.at.x - p.size_w / 2, p.at.y + p.size_h / 2);
            pcb::point p2(p.at.x + p.size_w / 2, p.at.y + p.size_h / 2);
            pcb::point p3(p.at.x + p.size_w / 2, p.at.y - p.size_h / 2);
            pcb::point p4(p.at.x - p.size_w / 2, p.at.y - p.size_h / 2);
            
            
            _pcb->get_rotation_pos(footprint.at, footprint.at_angle, p1);
            _pcb->get_rotation_pos(footprint.at, footprint.at_angle, p2);
            _pcb->get_rotation_pos(footprint.at, footprint.at_angle, p3);
            _pcb->get_rotation_pos(footprint.at, footprint.at_angle, p4);
            
            
            fprintf(fp, "p(1, 1) = %f; p(2, 1) = %f;\n", p1.x, p1.y);
            fprintf(fp, "p(1, 2) = %f; p(2, 2) = %f;\n", p2.x, p2.y);
            fprintf(fp, "p(1, 3) = %f; p(2, 3) = %f;\n", p3.x, p3.y);
            fprintf(fp, "p(1, 4) = %f; p(2, 4) = %f;\n", p4.x, p4.y);
        
            fprintf(fp, "CSX = AddLinPoly(CSX, '%s', 3, 2, %f, p, %f, 'CoordSystem', 0);\n", name.c_str(), z1, thickness);
            fprintf(fp, "clear p;\n");
            
            range.det(p1.x, p1.y);
            range.det(p2.x, p2.y);
            range.det(p3.x, p3.y);
            range.det(p4.x, p4.y);
            if (gen_mesh)
            {
                _mesh.x.insert(mesh::line(p1.x, mesh_prio)); _mesh.y.insert(mesh::line(p1.y, mesh_prio));
                _mesh.x.insert(mesh::line(p2.x, mesh_prio)); _mesh.y.insert(mesh::line(p2.y, mesh_prio));
                _mesh.x.insert(mesh::line(p3.x, mesh_prio)); _mesh.y.insert(mesh::line(p3.y, mesh_prio));
                _mesh.x.insert(mesh::line(p4.x, mesh_prio)); _mesh.y.insert(mesh::line(p4.y, mesh_prio));
            }
        }
        else if (p.shape == pcb::pad::SHAPE_CIRCLE)
        {
            pcb::point c(p.at);
            
            _pcb->get_rotation_pos(footprint.at, footprint.at_angle, c);
            float radius = p.size_w / 2;
            
            fprintf(fp, "CSX = AddCylinder(CSX, '%s', 3, [%f %f %f], [%f %f %f], %f);\n",
                        name.c_str(),
                        c.x, c.y, z1,
                        c.x, c.y, _ignore_cu_thickness? z2 + 0.001: z2,
                        radius);
                
            range.det(c.x, c.y);
            if (gen_mesh)
            {        
                _mesh.x.insert(mesh::line(c.x, mesh_prio));
                _mesh.y.insert(mesh::line(c.y, mesh_prio));
            }
        }
        else if (p.shape == pcb::pad::SHAPE_OVAL)
        {
            
        }
    }
    
    fprintf(fp, "\n\n");
    
}


void openems_model_gen::_add_excitation(FILE *fp, std::uint32_t mesh_prio)
{
    std::uint32_t portnr = 0;
    for (const auto& ex: _excitations)
    {
        fprintf(fp, "[CSX] = AddLumpedPort(CSX, 1, %u, %f, [%f %f %f], [%f %f %f], [%d %d %d], %s);\n",
                portnr,
                ex.R,
                ex.start.x, ex.start.y, ex.start.z,
                ex.end.x, ex.end.y, ex.end.z,
                (ex.dir == excitation::DIR_X)? 1: 0,
                (ex.dir == excitation::DIR_Y)? 1: 0,
                (ex.dir == excitation::DIR_Z)? 1: 0,
                (ex.excite)? "true": "false");
        portnr++;
        if (ex.gen_mesh)
        {
            _mesh.x.insert(mesh::line(ex.start.x, mesh_prio));
            _mesh.x.insert(mesh::line(ex.end.x, mesh_prio));
            _mesh.y.insert(mesh::line(ex.start.y, mesh_prio));
            _mesh.y.insert(mesh::line(ex.end.y, mesh_prio));
        }
    }
    fprintf(fp, "\n\n");
    fprintf(fp, "\n\n");
}


void openems_model_gen::_add_lumped_element(FILE *fp, std::uint32_t mesh_prio)
{
    for (const auto& element: _lumped_elements)
    {
        std::uint32_t dir = 0;
        if (element.dir == lumped_element::DIR_Y)
        {
            dir = 1;
        }
        else if (element.dir == lumped_element::DIR_Z)
        {
            dir = 2;
        }
        std::string type = "R";
        if (element.type == lumped_element::TYPE_R)
        {
            type = "R";
        }
        else if (element.type == lumped_element::TYPE_L)
        {
            type = "L";
        }
        else if (element.type == lumped_element::TYPE_C)
        {
            type = "C";
        }
        
        fprintf(fp, "[CSX] = AddLumpedElement(CSX, '%s', %d, 'Caps', 1, '%s', %g);\n", element.name.c_str(), dir, type.c_str(), element.v);
        fprintf(fp, "[CSX] = AddBox(CSX, '%s', 0, [%f %f %f], [%f %f %f]);\n",
                    element.name.c_str(),
                    element.start.x, element.start.y, element.start.z,
                    element.end.x, element.end.y, element.end.z);
        

        if (element.gen_mesh)
        {
            _mesh.x.insert(mesh::line(element.start.x, mesh_prio));
            _mesh.x.insert(mesh::line(element.end.x, mesh_prio));
            _mesh.y.insert(mesh::line(element.start.y, mesh_prio));
            _mesh.y.insert(mesh::line(element.end.y, mesh_prio));
        }
    }
    fprintf(fp, "\n\n");
    fprintf(fp, "\n\n");
    
}

void openems_model_gen::_add_nf2ff_box(FILE *fp, std::uint32_t mesh_prio)
{
    float nf2ff_cx = 0;
    float nf2ff_cy = 0;
    float nf2ff_cz = 0;
    
    if (_nf2ff_fp.empty())
    {
        return;
        if (!_excitations.empty())
        {
            auto ex = _excitations.front();
            nf2ff_cx = (ex.start.x + ex.end.x) / 2;
            nf2ff_cy = (ex.start.y + ex.end.y) / 2;
            nf2ff_cz = (ex.start.z + ex.end.z) / 2;
        }
    }
    else
    {
        pcb::footprint fp;
        if (_pcb->get_footprint(_nf2ff_fp, fp))
        {
            nf2ff_cz = _pcb->get_layer_z_axis(fp.layer);
            nf2ff_cx = fp.at.x;
            nf2ff_cy = fp.at.y;
        }
    }
    
    float ratio = (_bc == BC_PML)? _lambda_mesh_ratio / 10: _lambda_mesh_ratio;
    //float lambda = C0 / (_f0 + _fc) * 1e3;
    float lambda = C0 / (_far_field_freq) * 1e3;
    float x_margin = std::max(fabs(nf2ff_cx - _pcb->get_edge_left()) + lambda / ratio, fabs(nf2ff_cx - _pcb->get_edge_right())  + lambda / ratio);
    x_margin = std::max(x_margin, lambda / 2);
    float y_margin = std::max(fabs(nf2ff_cy - _pcb->get_edge_top()) + lambda / ratio, fabs(nf2ff_cy - _pcb->get_edge_bottom()) + lambda / ratio);
    y_margin = std::max(y_margin, lambda / 2);
    float z_margin = std::max(_pcb->get_board_thickness() * ratio, lambda / 2);
    
    _mesh.x.insert(mesh::line(nf2ff_cx - x_margin - lambda / ratio, mesh_prio));
    _mesh.x.insert(mesh::line(nf2ff_cx + x_margin + lambda / ratio, mesh_prio));
    _mesh.y.insert(mesh::line(nf2ff_cy - y_margin - lambda / ratio, mesh_prio));
    _mesh.y.insert(mesh::line(nf2ff_cy + y_margin + lambda / ratio, mesh_prio));
    _mesh.z.insert(mesh::line(nf2ff_cz - z_margin - lambda / ratio, mesh_prio));
    _mesh.z.insert(mesh::line(nf2ff_cz + z_margin + lambda / ratio, mesh_prio));
    
    fprintf(fp, "far_field_freq = %g;\n", _far_field_freq);
    fprintf(fp, "nf2ff_cx = %e; nf2ff_cy = %e; nf2ff_cz = %e;\n", nf2ff_cx, nf2ff_cy, nf2ff_cz);
    fprintf(fp, "x_margin = %e; y_margin = %e; z_margin = %e;\n", x_margin, y_margin, z_margin);
    fprintf(fp, "nf2ff_start = [nf2ff_cx - x_margin, nf2ff_cy - y_margin, nf2ff_cz - z_margin];\n");
    fprintf(fp, "nf2ff_stop = [nf2ff_cx + x_margin, nf2ff_cy + y_margin, nf2ff_cz + z_margin];\n");
    fprintf(fp, "[CSX nf2ff] = CreateNF2FFBox(CSX, 'nf2ff', nf2ff_start, nf2ff_stop);\n");


    fprintf(fp, "\n\n");
    fprintf(fp, "\n\n");
}


void openems_model_gen::_add_read_ui(FILE *fp)
{
    fprintf(fp, "freq = linspace(max([1e6, f0 - fc]), f0 + fc, 501);\n");
    for (std::uint32_t idx = 0; idx < _excitations.size(); idx++)
    {
        fprintf(fp, "U%u = ReadUI({'port_ut%u', 'et'}, [sim_path '/'], freq);\n", idx, idx);
        fprintf(fp, "I%u = ReadUI('port_it%u', [sim_path '/'], freq);\n", idx, idx);
        fprintf(fp, "\n\n");
    }
}


void openems_model_gen::_add_plot_feed_point_impedance(FILE *fp)
{
    std::uint32_t idx = 0;
    for (auto& ex: _excitations)
    {
        if (ex.excite == false)
        {
            continue;
        }
        fprintf(fp, "# plot feed point impedance\n");
        fprintf(fp, "figure\n");
        fprintf(fp, "Zin = U%u.FD{1}.val ./ I%u.FD{1}.val;\n", idx, idx);
        fprintf(fp, "plot(freq / 1e6, real(Zin), 'k-', 'Linewidth', 2);\n");
        fprintf(fp, "hold on\n");
        fprintf(fp, "grid on\n");
        fprintf(fp, "plot(freq/1e6, imag(Zin), 'r--', 'Linewidth', 2);\n");
        fprintf(fp, "title('feed point impedance');\n");
        fprintf(fp, "xlabel('frequency f / MHz');\n");
        fprintf(fp, "ylabel('impedance Z_{in} / Ohm');\n");
        fprintf(fp, "legend('real', 'imag');\n");
        fprintf(fp, "print('-dpng', [plot_path '/Zin_' num2str(%d) '.png']);\n", idx);
        
        fprintf(fp, "if exist('s11_min_freq_idx')\n");
        fprintf(fp, "    printf('freq:%%g Z(%%g + %%gi)\\n', freq(s11_min_freq_idx), real(Zin(s11_min_freq_idx)), imag(Zin(s11_min_freq_idx)));\n");
        fprintf(fp, "end\n");
        
        for (const auto& freq: _freq)
        {
            fprintf(fp, "freq_idx = find(freq > %g)(1) - 1;\n", freq);
            fprintf(fp, "printf('freq:%%g Z(%%g + %%gi)\\n', freq(freq_idx), real(Zin(freq_idx)), imag(Zin(freq_idx)));\n");
        }
        
        fprintf(fp, "printf('\\n\\n');\n");
        fprintf(fp, "\n\n");
        idx++;
    }
    
}


void openems_model_gen::_add_plot_s11(FILE *fp)
{
    std::uint32_t idx = 0;
    for (auto& ex: _excitations)
    {
        fprintf(fp, "# plot reflection coefficient S11\n");
        fprintf(fp, "figure\n");

        fprintf(fp, "uf_inc = 0.5*(U%u.FD{1}.val + I%u.FD{1}.val * %f);\n", idx, idx, ex.R);
        fprintf(fp, "if_inc = 0.5*(I%u.FD{1}.val - U%u.FD{1}.val / %f);\n", idx, idx, ex.R);
        fprintf(fp, "uf_ref = U%u.FD{1}.val - uf_inc;\n", idx);
        fprintf(fp, "if_ref = I%u.FD{1}.val - if_inc;\n", idx);
        
        fprintf(fp, "s11 = uf_ref ./ uf_inc;\n");
        fprintf(fp, "plot(freq / 1e6, 20 * log10(abs(s11)), 'k-', 'Linewidth', 2);\n");
        fprintf(fp, "grid on\n");
        fprintf(fp, "title('reflection coefficient S_{11} port%u');\n", idx);
        fprintf(fp, "xlabel('frequency f / MHz');\n");
        fprintf(fp, "ylabel('reflection coefficient |S_{11}|');\n");
        fprintf(fp, "print('-dpng', [plot_path '/S11_' num2str(%d) '.png']);\n", idx);
        
        fprintf(fp, "printf('\\n\\n');\n");
        fprintf(fp, "s11_db = 20 * log10(abs(s11));\n");
        
        {
            fprintf(fp, "freq_idx = find(s11==min(s11));\n");
            fprintf(fp, "s11_min_freq_idx = freq_idx;\n");
            
            fprintf(fp, "s11_db_left = s11_db(1:freq_idx);\n");
            fprintf(fp, "s11_db_right = s11_db(freq_idx:end);\n");
            fprintf(fp, "left_idx = find(s11_db_left >= -10)(end);\n");
            fprintf(fp, "right_idx = find(s11_db_right >= -10)(1);\n");
            
            fprintf(fp, "printf('Minimum S11 freq:%%g band width(%%g %%g)%%gMHz\\n'\n"
                            "    , freq(freq_idx), freq(left_idx), freq(freq_idx + right_idx)"
                            ", (freq(freq_idx + right_idx) - freq(left_idx)) / 1e6);\n");
        }
        
        for (const auto& freq: _freq)
        {
            fprintf(fp, "freq_idx = find(freq > %g)(1) - 1;\n", freq);
            
            fprintf(fp, "s11_db_left = s11_db(1:freq_idx);\n");
            fprintf(fp, "s11_db_right = s11_db(freq_idx:end);\n");
            fprintf(fp, "left_idx = find(s11_db_left >= -10)(end);\n");
            fprintf(fp, "right_idx = find(s11_db_right >= -10)(1);\n");
            
            
            
            fprintf(fp, "printf('freq:%%g band width(%%g %%g)%%gMHz\\n'\n"
                            "    , freq(freq_idx), freq(left_idx), freq(freq_idx + right_idx)"
                            ", (freq(freq_idx + right_idx) - freq(left_idx)) / 1e6);\n");
        }
        
        fprintf(fp, "printf('\\n\\n');\n");
        fprintf(fp, "\n\n");
        idx++;
    }
}

void openems_model_gen::_add_plot_two_sparamer(FILE *fp)
{
    if (_excitations.size() != 2)
    {
        printf("_add_plot_two_sparamer err.\n");
        return;
    }
    
    fprintf(fp, "# plot reflection coefficient S11/S21\n");

    fprintf(fp, "uf_inc0 = 0.5*(U0.FD{1}.val + I0.FD{1}.val * %f);\n", _excitations[0].R);
    fprintf(fp, "if_inc0 = 0.5*(I0.FD{1}.val - U0.FD{1}.val / %f);\n", _excitations[0].R);
    fprintf(fp, "uf_ref0 = U0.FD{1}.val - uf_inc0;\n");
    fprintf(fp, "if_ref0 = I0.FD{1}.val - if_inc0;\n");
    fprintf(fp, "s11 = uf_ref0 ./ uf_inc0;\n");
    
    fprintf(fp, "uf_inc1 = 0.5*(U1.FD{1}.val + I1.FD{1}.val * %f);\n", _excitations[0].R);
    fprintf(fp, "if_inc1 = 0.5*(I1.FD{1}.val - U1.FD{1}.val / %f);\n", _excitations[0].R);
    fprintf(fp, "uf_ref1 = U1.FD{1}.val - uf_inc1;\n");
    fprintf(fp, "if_ref1 = I1.FD{1}.val - if_inc1;\n");
    fprintf(fp, "s21 = uf_ref1 ./ uf_inc0;\n");
    
    fprintf(fp, "printf('\\n\\n');\n");
    
    fprintf(fp, "figure\n");
    fprintf(fp, "plot(freq / 1e6, 20 * log10(abs(s11)), 'k-', 'Linewidth', 2);\n");
    fprintf(fp, "hold on;\n");
    fprintf(fp, "grid on;\n");
    fprintf(fp, "plot(freq / 1e6, 20 * log10(abs(s21)), 'r--', 'Linewidth', 2);\n");
    fprintf(fp, "legend('S_{11}','S_{21}');\n");
    
    fprintf(fp, "ylabel('S-Parameter (dB)', 'FontSize',12);\n");
    fprintf(fp, "xlabel('frequency (MHz) \\rightarrow', 'FontSize', 12);\n");
    fprintf(fp, "print('-dpng', [plot_path '/S11_S21.png']);\n");
        
    fprintf(fp, "printf('\\n\\n');\n");
    fprintf(fp, "\n\n");
}
    
void openems_model_gen::_add_plot_vswr(FILE *fp)
{
    std::uint32_t idx = 0;
    for (auto& ex: _excitations)
    {
        (void)ex;
        fprintf(fp, "# plot vswr\n");
        fprintf(fp, "figure\n");

        fprintf(fp, "uf_inc = 0.5*(U%u.FD{1}.val + I%u.FD{1}.val * %f);\n", idx, idx, ex.R);
        fprintf(fp, "if_inc = 0.5*(I%u.FD{1}.val - U%u.FD{1}.val / %f);\n", idx, idx, ex.R);
        fprintf(fp, "uf_ref = U%u.FD{1}.val - uf_inc;\n", idx);
        fprintf(fp, "if_ref = I%u.FD{1}.val - if_inc;\n", idx);
        
        fprintf(fp, "s11 = uf_ref ./ uf_inc;\n");
        fprintf(fp, "vswr = (1 + abs(s11)) ./ (1 - abs(s11));\n");
        fprintf(fp, "plot(freq / 1e6, abs(vswr), 'k-', 'Linewidth', 2);\n");
        fprintf(fp, "set(gca, 'YScale', 'log');\n");
        fprintf(fp, "grid on\n");
        fprintf(fp, "title('vswr port%u');\n", idx);
        fprintf(fp, "xlabel('frequency f / MHz');\n");
        fprintf(fp, "ylabel('vswr');\n");
        fprintf(fp, "print('-dpng', [plot_path '/VSWR_' num2str(%d) '.png']);\n", idx);
        
        
        {
            fprintf(fp, "[vswr_min freq_idx] =  min(abs(vswr));\n");
            fprintf(fp, "printf('Minimum SWR: %%g@%%gMHz\\n', abs(vswr_min), freq(freq_idx) / 1e6);\n");
            
        }
        
        for (const auto& freq: _freq)
        {
            fprintf(fp, "freq_idx = find(freq > %g)(1) - 1;\n", freq);
            fprintf(fp, "printf('SWR: %%g@%%gMHz\\n', abs(vswr(freq_idx)), freq(freq_idx) / 1e6);\n");
        }
        fprintf(fp, "printf('\\n\\n');\n");
        
        fprintf(fp, "\n\n");
        idx++;
    }
}

void openems_model_gen::_add_plot_far_field(FILE *fp)
{
    if (_freq.empty())
    {
        fprintf(fp, "# NFFF contour plots\n");
        fprintf(fp, "f_res_ind = find(s11==min(s11));\n");
        fprintf(fp, "f_res = freq(f_res_ind);\n");
        fprintf(fp, "nf2ff = CalcNF2FF(nf2ff, sim_path, f_res, [-180: 2: 180] * pi / 180, [0 90] * pi / 180, 'Mode', 1, 'Center', (nf2ff_start + nf2ff_stop) * 0.5 * unit);\n");
        fprintf(fp, "figure\n");
        fprintf(fp, "polarFF(nf2ff, 'xaxis', 'theta', 'param', [1 2], 'logscale', -20, 'xtics', 5); drawnow;\n");
        
        fprintf(fp, "print('-dpng', [plot_path '/FF.png']);\n");
        
        fprintf(fp, "figure\n");
        fprintf(fp, "plotFFdB(nf2ff, 'xaxis', 'theta', 'param', [1 2]); drawnow;\n");
        fprintf(fp, "print('-dpng', [plot_path '/FFdB.png']);\n");
        
        fprintf(fp, "Dlog = 10 * log10(nf2ff.Dmax);\n");
        fprintf(fp, "disp(['radiated power: Prad = ' num2str(nf2ff.Prad) ' Watt']);\n");
        fprintf(fp, "disp(['directivity: Dmax = ' num2str(Dlog) ' dBi']);\n");
        
        
        std::uint32_t idx = 0;
        for (auto& ex: _excitations)
        {
            (void)ex;
            fprintf(fp, "uf_inc = 0.5*(U%u.FD{1}.val + I%u.FD{1}.val * %f);\n", idx, idx, ex.R);
            fprintf(fp, "if_inc = 0.5*(I%u.FD{1}.val - U%u.FD{1}.val / %f);\n", idx, idx, ex.R);
            fprintf(fp, "uf_ref = U%u.FD{1}.val - uf_inc;\n", idx);
            fprintf(fp, "if_ref = I%u.FD{1}.val - if_inc;\n", idx);
            fprintf(fp, "P_in = 0.5 * U%u.FD{1}.val .* conj(I%u.FD{1}.val);\n", idx, idx);
            
            fprintf(fp, "disp(['efficiency(port%u): nu_rad = ' num2str(100*nf2ff.Prad ./ real(P_in(f_res_ind))) ' %%']);\n", idx);
        }
        
        
        fprintf(fp, "# calculate 3D pattern\n");
        fprintf(fp, "phiRange = 0: 2: 360;\n");
        fprintf(fp, "thetaRange = 0: 2: 180;\n");
        fprintf(fp, "nf2ff = CalcNF2FF(nf2ff, sim_path, f_res, thetaRange*pi/180, phiRange*pi/180, 'Verbose', 2, 'Outfile', 'nf2ff_3D.h5', 'Mode', 1, 'Center', (nf2ff_start + nf2ff_stop) * 0.5 * unit);\n");
        fprintf(fp, "figure\n");
        fprintf(fp, "plotFF3D(nf2ff, 'logscale', -20); drawnow;\n");
        fprintf(fp, "print('-dpng', [plot_path '/FF3D.png']);\n");
        
        fprintf(fp, "E_far_normalized = nf2ff.E_norm{1} / max(nf2ff.E_norm{1}(:));\n");
        fprintf(fp, "DumpFF2VTK([sim_path '/FF_pattern.vtk'], E_far_normalized, thetaRange, phiRange);\n");

        fprintf(fp, "printf('\\n\\n');\n");
        fprintf(fp, "\n\n");
        return;
    }
    
    for (const auto& freq: _freq)
    {
        fprintf(fp, "# NFFF contour plots\n");
        fprintf(fp, "f_res_ind = find(freq > %g)(1) - 1;\n", freq);
        //fprintf(fp, "f_res_ind = find(s11==min(s11));\n");
        fprintf(fp, "f_res = freq(f_res_ind);\n");
        fprintf(fp, "nf2ff = CalcNF2FF(nf2ff, sim_path, f_res, [-180: 2: 180] * pi / 180, [0 90] * pi / 180, 'Mode', 1, 'Center', (nf2ff_start + nf2ff_stop) * 0.5 * unit);\n");
        fprintf(fp, "figure\n");
        fprintf(fp, "polarFF(nf2ff, 'xaxis', 'theta', 'param', [1 2], 'logscale', -20, 'xtics', 5); drawnow;\n");
        fprintf(fp, "print('-dpng', [plot_path '/FF.png']);\n");
        
        fprintf(fp, "figure\n");
        fprintf(fp, "plotFFdB(nf2ff, 'xaxis', 'theta', 'param', [1 2]); drawnow;\n");
        fprintf(fp, "print('-dpng', [plot_path '/FFdB.png']);\n");
        
        fprintf(fp, "Dlog = 10 * log10(nf2ff.Dmax);\n");
        fprintf(fp, "disp(['radiated power: Prad = ' num2str(nf2ff.Prad) ' Watt']);\n");
        fprintf(fp, "disp(['directivity: Dmax = ' num2str(Dlog) ' dBi']);\n");
        
        
        std::uint32_t idx = 0;
        for (auto& ex: _excitations)
        {
            (void)ex;
            fprintf(fp, "uf_inc = 0.5*(U%u.FD{1}.val + I%u.FD{1}.val * %f);\n", idx, idx, ex.R);
            fprintf(fp, "if_inc = 0.5*(I%u.FD{1}.val - U%u.FD{1}.val / %f);\n", idx, idx, ex.R);
            fprintf(fp, "uf_ref = U%u.FD{1}.val - uf_inc;\n", idx);
            fprintf(fp, "if_ref = I%u.FD{1}.val - if_inc;\n", idx);
            fprintf(fp, "P_in = 0.5 * U%u.FD{1}.val .* conj(I%u.FD{1}.val);\n", idx, idx);
            
            fprintf(fp, "disp(['efficiency(port%u): nu_rad = ' num2str(100*nf2ff.Prad ./ real(P_in(f_res_ind))) ' %%']);\n", idx);
        }
        
        
        fprintf(fp, "# calculate 3D pattern\n");
        fprintf(fp, "phiRange = 0: 2: 360;\n");
        fprintf(fp, "thetaRange = 0: 2: 180;\n");
        fprintf(fp, "nf2ff = CalcNF2FF(nf2ff, sim_path, f_res, thetaRange*pi/180, phiRange*pi/180, 'Verbose', 2, 'Outfile', 'nf2ff_3D.h5', 'Mode', 1, 'Center', (nf2ff_start + nf2ff_stop) * 0.5 * unit);\n");
        fprintf(fp, "figure\n");
        fprintf(fp, "plotFF3D(nf2ff, 'logscale', -20); drawnow;\n");
        fprintf(fp, "print('-dpng', [plot_path '/FF3D.png']);\n");
        
        fprintf(fp, "E_far_normalized = nf2ff.E_norm{1} / max(nf2ff.E_norm{1}(:));\n");
        fprintf(fp, "DumpFF2VTK([sim_path '/FF_pattern.vtk'], E_far_normalized, thetaRange, phiRange);\n");

        fprintf(fp, "printf('\\n\\n');\n");
        fprintf(fp, "\n\n");
    }
}

void openems_model_gen::_apply_mesh_line_range(mesh& mesh)
{
    _apply_mesh_line_range(mesh.x, mesh.x_range);
    _apply_mesh_line_range(mesh.y, mesh.y_range);
    _apply_mesh_line_range(mesh.z, mesh.z_range);
}

void openems_model_gen::_apply_mesh_line_range(std::set<mesh::line>& mesh_line, const std::multiset<mesh::line_range>& mesh_line_range)
{
    for (auto it = mesh_line_range.begin(); it != mesh_line_range.end(); it++)
    {
        float start = it->start;
        float end = it->end;
        float gap = it->gap;
        
        for (float v = start; v < end; v += gap)
        {
            bool continue_ = false;
            for (auto it2 = mesh_line_range.begin(); it2 != it; it2++)
            {
                if (v >= it2->start && v < it2->end)
                {
                    continue_ = true;
                    break;
                }
            }
            if (continue_)
            {
                continue;
            }
            mesh_line.insert(mesh::line(v, it->prio));
        }
    }
}

void openems_model_gen::_clean_mesh_line(std::set<mesh::line>& mesh_line, float min_gap)
{
    if (mesh_line.size() < 2)
    {
        return;
    }
    
    bool brk = true;
    do
    {
        brk = true;
        auto it1 = mesh_line.begin();
        auto it2 = it1;
        it2++;
        for (; it2 != mesh_line.end(); )
        {
            if (fabs(it2->v - it1->v) < min_gap/* && (it2->prio < 99 || it1->prio < 99)*/)
            {
                brk = false;
                if (it1->prio == it2->prio)
                {
                    mesh::line tmp((it1->v + it2->v) * 0.5, it1->prio);
                    
                    it1 = mesh_line.erase(it1);
                    it1 = mesh_line.erase(it1);
                    auto r = mesh_line.insert(tmp);
                    if (r.second)
                    {
                        it1 = r.first;
                    }
                }
                else if (it1->prio > it2->prio)
                {
                    it1 = mesh_line.erase(it2);
                }
                else
                {
                    it1 = mesh_line.erase(it1);
                }
                it2 = it1;
                it2++;
                continue;
            }
            it1++;
            it2++;
        }
    } while (brk == false);
}


std::vector<pcb::point> openems_model_gen::_get_fp_poly_points(const pcb::footprint& fp, const std::string& pad_number)
{
    std::vector<pcb::point> points;
    for (const auto& p: fp.pads)
    {
        if (p.pad_number != pad_number)
        {
            continue;
        }
        if (p.shape == pcb::pad::SHAPE_RECT || p.shape == pcb::pad::SHAPE_ROUNDRECT)
        {
            pcb::point p1(p.at.x - p.size_w / 2, p.at.y + p.size_h / 2);
            pcb::point p2(p.at.x + p.size_w / 2, p.at.y + p.size_h / 2);
            pcb::point p3(p.at.x + p.size_w / 2, p.at.y - p.size_h / 2);
            pcb::point p4(p.at.x - p.size_w / 2, p.at.y - p.size_h / 2);
            
            
            _pcb->get_rotation_pos(fp.at, fp.at_angle, p1);
            _pcb->get_rotation_pos(fp.at, fp.at_angle, p2);
            _pcb->get_rotation_pos(fp.at, fp.at_angle, p3);
            _pcb->get_rotation_pos(fp.at, fp.at_angle, p4);
            
            points.push_back(p1);
            points.push_back(p2);
            points.push_back(p3);
            points.push_back(p4);
        }
        else if (p.shape == pcb::pad::SHAPE_CIRCLE)
        {
            pcb::point c(p.at);
            
            _pcb->get_rotation_pos(fp.at, fp.at_angle, c);
            float radius = p.size_w / 2;
            points.push_back(pcb::point(c.x - radius, c.y));
            points.push_back(pcb::point(c.x + radius, c.y));
            points.push_back(pcb::point(c.x, c.y - radius));
            points.push_back(pcb::point(c.x, c.y + radius));
        }
    }
    return points;
}

float openems_model_gen::_round_xy(float v)
{
    return roundf(v * 10.) / 10.;
}


float openems_model_gen::_suffix_to_value(const std::string& suffix)
{
    switch (suffix[0])
    {
        case 'T': return 1e12;
        case 'G': return 1e9;
        case 'M': return 1e6;
        case 'k': return 1e3;
        case 'm': return 1e-3;
        case 'u': return 1e-6;
        case 'n': return 1e-9;
        case 'p': return 1e-12;
        case 'f': return 1e-15;
    }
    return 1;
}

float openems_model_gen::_string_to_float(const std::string& str)
{
    std::string num;
    std::string suffix;
    for (const auto& c: str)
    {
        if (c == '+' || c == '-' || c == '.'
            || c == 'e' || c == 'E' 
            || (c >= '0' && c <= '9'))
        {
            num.push_back(c);
        }
        else
        {
            break;
        }
    }
    suffix = str.substr(num.size(), str.npos);
    
    return std::atof(num.c_str()) * _suffix_to_value(suffix);
    
}