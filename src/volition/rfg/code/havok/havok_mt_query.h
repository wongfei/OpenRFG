// Original: volition/rfg/code/havok/havok_mt_query.h
// Implementation: volition/rfg/code/havok/havok_mt_query.cpp
//
// Multithreaded pair linear casts: a collidable is cast against a list of collidables on a worker job
// (havok_mt_query_pair_linearcast), and havok_mt_query_wait waits for the results.
#pragma once


class hkVector4f;
class hkpCdPointCollector;
class hkpCollidable;
class hkpWorld;

void havok_mt_query_initialize(hkpWorld* p_world); // 0x00811B70 (:234)
void havok_mt_query_pair_linearcast(const hkpCollidable* p_collidable_to_cast, const hkVector4f& destination_position, const hkpCollidable* const* p_collidables_to_cast_against, unsigned int num_collidables_to_cast_against, unsigned int collidables_to_cast_against_stride, float tolerance, float max_extra_penetration, hkpCdPointCollector& p_cast_collector, hkpCdPointCollector* p_start_collector); // 0x0082B280 (:260)
void havok_mt_query_wait(); // 0x00811B80 (:298)
