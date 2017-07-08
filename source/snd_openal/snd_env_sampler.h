#ifndef QFUSION_SND_EFX_MANAGER_H
#define QFUSION_SND_EFX_MANAGER_H

#include "../gameshared/q_collision.h"

struct src_s;

void ENV_Init();
void ENV_Shutdown();

void ENV_UpdateListener( const vec3_t origin, const vec3_t velocity );

void ENV_RegisterSource( struct src_s *src );

void ENV_UnregisterSource( struct src_s *src );

#endif
