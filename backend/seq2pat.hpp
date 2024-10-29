// -*- coding: utf-8 -*-
// SPDX-License-Identifier: GPL-2.0

#ifndef SEQ2PAT_H
#define SEQ2PAT_H

#include <vector>
#include <iostream>
#include <string>

// This class holds all the parameters
// and the method to perform mining
namespace patterns {
    class Seq2pat {
        public:

            // Parameters
            std::string out_file;
            int num_att;                                          
            std::vector<int>  lgap, ugap, lavr, uavr, lspn, uspn, lmed, umed;             
            std::vector<int> ugapi, lgapi, uspni, lspni, uavri, lavri, umedi, lmedi;     
            std::vector<int> num_minmax, num_avr, num_med;                   
            std::vector<int> tot_gap, tot_spn, tot_avr;
            std::vector<std::vector<int> > patterns;                       
            int N, M, L, theta;
            std::vector<std::vector<int> > items;                                    
            std::vector<std::vector<std::vector<int> > > attrs;
            std::vector<int> max_attrs, min_attrs;
            int max_number_of_pattern;

            // Class object
            Seq2pat();
            ~Seq2pat();

            // Mining function
            std::vector< std::vector<int> > mine();
    };
}

#endif

