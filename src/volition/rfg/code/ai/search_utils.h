// Original: volition/rfg/code/ai/search_utils.h
// Implementation: volition/rfg/code/ai/search_utils.cpp
//
// Search helpers: the next location to search and the cover node to search it from.
#pragma once


class cover_node;
class human;
struct nav_cell;
class npc;
class vector;

bool ai_search_get_next_search_location(vector& pos, vector& direct_pos, nav_cell*& ncp, const npc* np, const human* target); // 0x00686670 (:690)
cover_node* ai_search_get_cover_node_for_location(const npc* np, const vector& pos, const vector& direct_pos, const nav_cell* ncp); // 0x0066BD60 (:715)
bool ai_search_adjust_direct_pos_from_cover_node(vector& adjusted_direct_pos, const npc* np, const cover_node* c_node, const vector& pos, const vector& old_direct_pos, const nav_cell* ncp); // 0x005F1C20 (:891)
