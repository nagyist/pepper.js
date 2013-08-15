/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** @file hello_world.c
 * This example demonstrates loading, running and scripting a very simple
 * NaCl module.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_view.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"

#if defined(__native_client__) || defined(__EMSCRIPTEN__)
#if defined(__CLANG__)
#define TCNAME "pnacl"
#elif defined(__GLIBC__)
#define TCNAME "glibc"
#else
#define TCNAME "newlib"
#endif
#else
#define TCNAME "host"
#endif

static PPB_Core* ppb_core_interface = NULL;
static PPB_Console* ppb_console_interface = NULL;
static PPB_Instance* ppb_instance_interface = NULL;
static PPB_Messaging* ppb_messaging_interface = NULL;
static PPB_Var* ppb_var_interface = NULL;
static PPB_View* ppb_view_interface = NULL;
static PPB_Graphics2D* ppb_graphics_2d_interface = NULL;
static PPB_ImageData* ppb_image_data_interface = NULL;

PP_Resource g2d = 0;
PP_Resource img = 0;
unsigned int* img_data = NULL;
int width = 0;
int height = 0;
PP_TimeTicks last;
float phase = 0;

PP_Instance inst = 0;

extern void Draw();


/**
 * Creates new string PP_Var from C string. The resulting object will be a
 * refcounted string object. It will be AddRef()ed for the caller. When the
 * caller is done with it, it should be Release()d.
 * @param[in] str C string to be converted to PP_Var
 * @return PP_Var containing string.
 */
static struct PP_Var CStrToVar(const char* str) {
  if (ppb_var_interface != NULL) {
    return ppb_var_interface->VarFromUtf8(str, strlen(str));
  }
  return PP_MakeUndefined();
}


/**
 * Post a message back to our JavaScript
 */
static void SendMessage(PP_Instance instance, const char *str) {
  if (ppb_messaging_interface)
    ppb_messaging_interface->PostMessage(instance, CStrToVar(str));
}

/**
 * Send a message to the JavaScript Console
 */
static void LogMessage(PP_Instance instance, const char *str) {
  if (ppb_console_interface)
    ppb_console_interface->Log(instance, PP_LOGLEVEL_ERROR,
                          CStrToVar(str));
}

/**
 * Called when the NaCl module is instantiated on the web page. The identifier
 * of the new instance will be passed in as the first argument (this value is
 * generated by the browser and is an opaque handle).  This is called for each
 * instantiation of the NaCl module, which is each time the <embed> tag for
 * this module is encountered.
 *
 * If this function reports a failure (by returning @a PP_FALSE), the NaCl
 * module will be deleted and DidDestroy will be called.
 * @param[in] instance The identifier of the new instance representing this
 *     NaCl module.
 * @param[in] argc The number of arguments contained in @a argn and @a argv.
 * @param[in] argn An array of argument names.  These argument names are
 *     supplied in the <embed> tag, for example:
 *       <embed id="nacl_module" dimensions="2">
 *     will produce two arguments, one named "id" and one named "dimensions".
 * @param[in] argv An array of argument values.  These are the values of the
 *     arguments listed in the <embed> tag.  In the above example, there will
 *     be two elements in this array, "nacl_module" and "2".  The indices of
 *     these values match the indices of the corresponding names in @a argn.
 * @return @a PP_TRUE on success.
 */
static PP_Bool Instance_DidCreate(PP_Instance instance,
                                  uint32_t argc,
                                  const char* argn[],
                                  const char* argv[]) {

  inst = instance;

  const char* post_msg = "Hello World (" TCNAME ")!";
  const char* console_msg = "Hello World (JavaScript Console)!";

  SendMessage(instance, post_msg);
  LogMessage(instance, console_msg);

  // Dump the argument to the console.
  char buffer[1024];
  for (int i = 0; i < argc; i++) {
    snprintf(buffer, sizeof(buffer), "Arg %d: %s = %s", i, argn[i], argv[i]);
    LogMessage(instance, buffer);
  }

  return PP_TRUE;
}


/**
 * Called when the NaCl module is destroyed. This will always be called,
 * even if DidCreate returned failure. This routine should deallocate any data
 * associated with the instance.
 * @param[in] instance The identifier of the instance representing this NaCl
 *     module.
 */
static void Instance_DidDestroy(PP_Instance instance) {
}

/**
 * Called when the position, the size, or the clip rect of the element in the
 * browser that corresponds to this NaCl module has changed.
 * @param[in] instance The identifier of the instance representing this NaCl
 *     module.
 * @param[in] position The location on the page of this NaCl module. This is
 *     relative to the top left corner of the viewport, which changes as the
 *     page is scrolled.
 * @param[in] clip The visible region of the NaCl module. This is relative to
 *     the top left of the plugin's coordinate system (not the page).  If the
 *     plugin is invisible, @a clip will be (0, 0, 0, 0).
 */
static void Instance_DidChangeView(PP_Instance instance,
                                   PP_Resource view_resource) {
  PP_Rect rect;
  PP_Rect clip;
  ppb_view_interface->GetRect(view_resource, &rect);
  PP_Bool fullscreen = ppb_view_interface->IsFullscreen(view_resource);
  PP_Bool visible = ppb_view_interface->IsVisible(view_resource);
  PP_Bool page_visible = ppb_view_interface->IsPageVisible(view_resource);
  ppb_view_interface->GetClipRect(view_resource, &clip);
  float device_scale = ppb_view_interface->GetDeviceScale(view_resource);
  float css_scale = ppb_view_interface->GetCSSScale(view_resource);

  char buffer[1024];
  snprintf(buffer, sizeof buffer, "view rect=(x=%d, y=%d, w=%d, h=%d) "
           "full=%d vis=%d page_vis=%d "
           "clip=(x=%d, y=%d, w=%d, h=%d) dev_scale=%f css_scale=%f",
           rect.point.x, rect.point.y, rect.size.width, rect.size.height,
           fullscreen, visible, page_visible,
           clip.point.x, clip.point.y, clip.size.width, clip.size.height,
           device_scale, css_scale);
  LogMessage(instance, buffer);


  PP_Size size;
  size = rect.size;

  if (size.width != width || size.height != height) {
    width = size.width;
    height = size.height;

    if (g2d != 0) {
      ppb_core_interface->ReleaseResource(g2d);
    }
    g2d = ppb_graphics_2d_interface->Create(instance, &size, PP_TRUE);
    ppb_instance_interface->BindGraphics(instance, g2d);

    if (img != 0) {
      ppb_core_interface->ReleaseResource(img);
    }
    img = ppb_image_data_interface->Create(instance, PP_IMAGEDATAFORMAT_RGBA_PREMUL, &size, PP_TRUE);

    img_data = (unsigned int*)ppb_image_data_interface->Map(img);

    last = ppb_core_interface->GetTimeTicks();
    Draw();
  }
}

void Flushed(void* user_data, int32_t result) {
  Draw();
}

void Draw() {
  PP_TimeTicks current = ppb_core_interface->GetTimeTicks();
  float dt = (float)(current - last);
  last = current;

  phase += dt * 2.0;
  while (phase > 3.0) {
    phase -= 3.0;
  }

  int color0;
  int color1;

  if (phase < 1.0) {
    color0 = 0xff0000ff;
    color1 = 0xff00ff00;
  } else if (phase < 2.0) {
    color0 = 0xff00ff00;
    color1 = 0xffff0000;
  } else {
    color0 = 0xffff0000;
    color1 = 0xff0000ff;
  }

  //char buffer[1024];
  //snprintf(buffer, sizeof buffer, "phase=%f dt=%f current=%f color0=%d color1=%d", phase, dt, current, color0, color1);
  //LogMessage(inst, buffer);

  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      if(((i>>3)+(j>>3))& 1) {
        img_data[j * width + i] = color0;
      } else {
        img_data[j * width + i] = color1;
      }
    }
  }

  PP_Point delta;
  delta.x = 3;
  delta.y = 2;
  PP_Rect clip;
  clip.point.x = 10;
  clip.point.y = 10;
  clip.size.width = width - 20;
  clip.size.height = height - 20;
  ppb_graphics_2d_interface->Scroll(g2d, &clip, &delta);


  PP_Point pos;
  pos.x = 20;
  pos.y = 20;
  PP_Rect src;
  src.point.x = 4+16;
  src.point.y = 4+16;
  src.size.width = 64;
  src.size.height = 64;

  ppb_graphics_2d_interface->PaintImageData(g2d, img, &pos, &src);

  PP_CompletionCallback callback = PP_MakeCompletionCallback(Flushed, NULL);
  ppb_graphics_2d_interface->Flush(g2d, callback);
}

/**
 * Notification that the given NaCl module has gained or lost focus.
 * Having focus means that keyboard events will be sent to the NaCl module
 * represented by @a instance. A NaCl module's default condition is that it
 * will not have focus.
 *
 * Note: clicks on NaCl modules will give focus only if you handle the
 * click event. You signal if you handled it by returning @a true from
 * HandleInputEvent. Otherwise the browser will bubble the event and give
 * focus to the element on the page that actually did end up consuming it.
 * If you're not getting focus, check to make sure you're returning true from
 * the mouse click in HandleInputEvent.
 * @param[in] instance The identifier of the instance representing this NaCl
 *     module.
 * @param[in] has_focus Indicates whether this NaCl module gained or lost
 *     event focus.
 */
static void Instance_DidChangeFocus(PP_Instance instance,
                                    PP_Bool has_focus) {
}

/**
 * Handler that gets called after a full-frame module is instantiated based on
 * registered MIME types.  This function is not called on NaCl modules.  This
 * function is essentially a place-holder for the required function pointer in
 * the PPP_Instance structure.
 * @param[in] instance The identifier of the instance representing this NaCl
 *     module.
 * @param[in] url_loader A PP_Resource an open PPB_URLLoader instance.
 * @return PP_FALSE.
 */
static PP_Bool Instance_HandleDocumentLoad(PP_Instance instance,
                                           PP_Resource url_loader) {
  /* NaCl modules do not need to handle the document load function. */
  return PP_FALSE;
}



/**
 * Entry points for the module.
 * Initialize needed interfaces: PPB_Core, PPB_Messaging and PPB_Var.
 * @param[in] a_module_id module ID
 * @param[in] get_browser pointer to PPB_GetInterface
 * @return PP_OK on success, any other value on failure.
 */
PP_EXPORT int32_t PPP_InitializeModule(PP_Module a_module_id,
                                       PPB_GetInterface get_browser) {
  ppb_core_interface =
      (PPB_Core*)(get_browser(PPB_CORE_INTERFACE));
  ppb_console_interface =
      (PPB_Console*)(get_browser(PPB_CONSOLE_INTERFACE));
  ppb_instance_interface =
      (PPB_Instance*)(get_browser(PPB_INSTANCE_INTERFACE));

  ppb_messaging_interface =
      (PPB_Messaging*)(get_browser(PPB_MESSAGING_INTERFACE));
  ppb_var_interface = (PPB_Var*)(get_browser(PPB_VAR_INTERFACE));
  ppb_view_interface = (PPB_View*)(get_browser(PPB_VIEW_INTERFACE));

  ppb_graphics_2d_interface = (PPB_Graphics2D*)(get_browser(PPB_GRAPHICS_2D_INTERFACE));
  ppb_image_data_interface = (PPB_ImageData*)(get_browser(PPB_IMAGEDATA_INTERFACE));

  return PP_OK;
}


/**
 * Returns an interface pointer for the interface of the given name, or NULL
 * if the interface is not supported.
 * @param[in] interface_name name of the interface
 * @return pointer to the interface
 */
PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0) {
    static PPP_Instance instance_interface = {
      &Instance_DidCreate,
      &Instance_DidDestroy,
      &Instance_DidChangeView,
      &Instance_DidChangeFocus,
      &Instance_HandleDocumentLoad,
    };
    return &instance_interface;
  }
  return NULL;
}


/**
 * Called before the plugin module is unloaded.
 */
PP_EXPORT void PPP_ShutdownModule() {
}
