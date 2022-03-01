/*
OBS Shared Memory
Copyright (C) 2022 Marzocchi Alessandro alessandro.marzocchi@gmail.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "plugin-macros.generated.h"
#include "SharedImage/sharedimage.h"
#include <obs-module.h>
#include <obs-source.h>
/* Defines common functions (required) */
OBS_DECLARE_MODULE()

/* Implements common ini-based locale (optional) */
OBS_MODULE_USE_DEFAULT_LOCALE("SharedMemory", "en-US")

typedef struct
{
    char *shared_id;
    float timeout;
    float elapsedFromLast;
    struct SharedImage *shared;
    gs_texture_t *texture;
    uint32_t textureWidth;
    uint32_t textureHeight;
    uint32_t textureBytesPerLine;
} OBSSharedMemoryData;

static uint32_t shared_width(void *data)
{
    return ((OBSSharedMemoryData *)data)->textureWidth;
}

static uint32_t shared_height(void *data)
{
    return ((OBSSharedMemoryData *)data)->textureHeight;
}

static const char *shared_name(void *data)
{
    return "Shared memory";
}

static void shared_update(void *data, obs_data_t *settings)
{
    OBSSharedMemoryData *shared=(OBSSharedMemoryData *)data;
    shared->timeout=obs_data_get_int(settings, "timeout")*1e-3;
    const char *id=obs_data_get_string(settings, "shared_id");
    if(id && (!shared->shared_id || strcmp(shared->shared_id, id)))
    {
        if(shared->shared_id)
            bfree(shared->shared_id);
        shared->shared_id=bstrdup(id);
        shared->elapsedFromLast=INFINITY;
        if(!sharedImageCreate(shared->shared_id, &shared->shared, 3000*2000, false))
        {
            sharedImageDestroy(shared->shared);
            shared->shared=NULL;
        }
    }
    fflush(stdout);
}

static void *shared_create(obs_data_t *settings, obs_source_t *source)
{
    OBSSharedMemoryData *ret=bzalloc(sizeof(OBSSharedMemoryData));
    ret->textureWidth=0;
    ret->textureHeight=0;
    ret->textureBytesPerLine=0;
    ret->elapsedFromLast=INFINITY;
    shared_update(ret, settings);
    return ret;
}


static void shared_destroy(void *data)
{
    OBSSharedMemoryData *obj=(OBSSharedMemoryData *)data;
    sharedImageDestroy(obj->shared);
    if(obj->texture)
        gs_texture_destroy(obj->texture);
    bfree(data);
}

static void shared_render(void *data, gs_effect_t *effect)
{
    OBSSharedMemoryData *obj=(OBSSharedMemoryData *)data;
    if(obj->texture && obj->elapsedFromLast<obj->timeout)
        obs_source_draw(obj->texture, 0, 0, 0, 0, false);
}
static void shared_video_tick(void *data, float seconds)
{
    OBSSharedMemoryData *shared=(OBSSharedMemoryData *)data;
    shared->elapsedFromLast+=seconds;
    if(shared->shared)
    {
        const SharedImageSetting *setting;
        void *imageData;
        if(sharedImageReceive(shared->shared, &imageData, &setting))
        {
            shared->elapsedFromLast=0;
            obs_enter_graphics();
            if(!shared->texture || shared->textureWidth!=setting->width || shared->textureHeight!=setting->height)
            {
                // We need to create or update the texture size
                if(shared->texture)
                    gs_texture_destroy(shared->texture);
                shared->texture=gs_texture_create(setting->width, setting->height, GS_RGBA, 1, NULL, GS_DYNAMIC);
                shared->textureWidth=setting->width;
                shared->textureHeight=setting->height;
            }
            if(shared->texture)
            {
                gs_texture_set_image(shared->texture, imageData, setting->bytesPerLine, false);
            }
            obs_leave_graphics();
        }
    }
}

obs_properties_t *shared_get_properties(void *unused)
{
    UNUSED_PARAMETER(unused);
    obs_properties_t *ret;
    ret = obs_properties_create();
    obs_properties_add_text(ret, "shared_id", "Shared memory id", OBS_TEXT_DEFAULT);
    obs_properties_add_int(ret, "timeout", "Timeout [ms]", 0, 60000, 100);
    return ret;
}

void shared_get_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, "shared_id", "sharedimg");
    obs_data_set_default_int(settings, "timeout", 800);
}

struct obs_source_info shared_source ={
    .id            = "Shared Memory",
    .type          = OBS_SOURCE_TYPE_INPUT,
    .output_flags  = OBS_SOURCE_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE,
    .get_name      = shared_name,
    .create        = shared_create,
    .destroy       = shared_destroy,
    .get_width     = shared_width,
    .get_height    = shared_height,
    .video_tick    = shared_video_tick,
    .video_render  = shared_render,
    .get_properties= shared_get_properties,
    .get_defaults  = shared_get_defaults,
    .update        = shared_update,
    .icon_type     = OBS_ICON_TYPE_SLIDESHOW
};

MODULE_EXPORT const char *obs_module_description(void)
{
    return "Shared memory overlay provider";
}

bool obs_module_load(void)
{
    blog(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
    obs_register_source(&shared_source);
    return true;
}

void obs_module_unload()
{
    blog(LOG_INFO, "plugin unloaded");
}
