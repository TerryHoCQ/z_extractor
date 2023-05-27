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

#ifndef __OPENEMS_MODEL_GEN_H__
#define __OPENEMS_MODEL_GEN_H__

#include <set>
#include <vector>
#include "pcb.h"

class openems_model_gen
{
public:
    struct point
    {
        point() : x(0), y(0), z(0) {}
        point(float x, float y, float z) : x(x), y(y), z(z) {}
        float x;
        float y;
        float z;
    };
    
    struct excitation
    {
        enum
        {
            DIR_X,
            DIR_Y,
            DIR_Z,
        };
        
        excitation()
            : dir(DIR_X)
            , R(50)
        {
        }
        
        std::string footprint1;
        std::string footprint1_pad_number;
        std::string footprint1_layer_name;
        
        std::string footprint2;
        std::string footprint2_pad_number;
        std::string footprint2_layer_name;
        
        point start;
        point end;
        
        std::uint32_t dir;
        float R;
    };
    
public:
    openems_model_gen(const std::shared_ptr<pcb>& pcb);
    ~openems_model_gen();
    
public:
    void add_net(std::uint32_t net_id);
    void add_footprint(const std::string& fp_ref);
    void add_excitation(const std::string& fp1, const std::string& fp1_pad_number, const std::string& fp1_layer_name,
                        const std::string& fp2, const std::string& fp2_pad_number, const std::string& fp2_layer_name, std::uint32_t dir, float R = 50);
                        
    void add_excitation(pcb::point start, const std::string& start_layer, pcb::point end, const std::string& end_layer, std::uint32_t dir, float R = 50);
                        
    void set_nf2ff(const std::string& fp);
    void set_excitation_freq(float f0, float fc);
    void set_far_field_freq(float freq);
    void gen_model(const std::string& func_name);
    void gen_mesh(const std::string& func_name);
    void gen_antenna_simulation_scripts();
private:
    void _gen_mesh_z(FILE *fp);
    void _gen_mesh_xy(FILE *fp);
    void _add_dielectric(FILE *fp);
    void _add_metal(FILE *fp);
    void _add_segment(FILE *fp);
    void _add_via(FILE *fp);
    void _add_zone(FILE *fp);
    void _add_footprint(FILE *fp);
    void _add_gr(const pcb::gr& gr, pcb::point at, float angle, const std::string& name, FILE *fp);
    void _add_pad(const pcb::footprint& footprint, const pcb::pad& p, const std::string& name, FILE *fp);
    
    void _add_excitation(FILE *fp);
    void _add_nf2ff_box(FILE *fp);
    
    void _clean_mesh_lines(std::set<float>& mesh_lines, float min_gap = 0.01);
private:
    std::shared_ptr<pcb> _pcb;
    std::set<std::uint32_t> _nets;
    std::set<std::string> _footprints;
    
    std::set<float> _mesh_x;
    std::set<float> _mesh_y;
    std::set<float> _mesh_z;
    
    bool _ignore_cu_thickness;
    
    float _f0;
    float _fc;
    float _far_field_freq;
    std::string _nf2ff_fp;
    
    std::vector<excitation> _excitations;
};

#endif