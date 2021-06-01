#include <stdio.h>
#include "libIrrlicht-clib.h"

#ifdef __cplusplus
#error "This is supposed to be compiled as C!"
#endif

int main(int argc, char *argv[]) {
  upp_irr_core_dimension2d_unsigned_int windowSize = {800, 600};
  upp_irr_IrrlichtDevice *device = createDevice(upp_irr_video_EDT_OPENGL,
                                                &windowSize,
                                                16,
                                                false,
                                                false,
                                                true,
                                                0);

  upp_irr_IrrlichtDevice_setWindowCaption(device, L"Hello World!");
  upp_irr_video_IVideoDriver *driver = upp_irr_IrrlichtDevice_getVideoDriver(device);
  upp_irr_scene_ISceneManager *manager = upp_irr_IrrlichtDevice_getSceneManager(device);
  upp_irr_core_vector3df pos = {0, 30, -40};
  upp_irr_core_vector3df lookat = {0, 5, 0};
  upp_irr_scene_ISceneManager_addCameraSceneNode(manager, 0, &pos, &lookat, -1, true);

  upp_irr_video_SColor *color = upp_new_irr_video_SColor_2(255,100,101,140);
  while(upp_irr_IrrlichtDevice_run(device)) {
    upp_irr_video_SExposedVideoData *vd = upp_new_irr_video_SExposedVideoData();
    upp_irr_video_IVideoDriver_beginScene(driver, true, true, color, vd, 0);
    upp_irr_scene_ISceneManager_drawAll(manager);
    upp_irr_video_IVideoDriver_endScene(driver);
    upp_del_irr_video_SExposedVideoData(vd);
  }
  upp_del_irr_video_SColor(color);
  // Using a superclass method requires a cast
  upp_irr_IReferenceCounted_drop((upp_irr_IReferenceCounted *)device);
  return 0;
}
