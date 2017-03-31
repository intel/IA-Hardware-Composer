#include "jsonhandlers.h"
#include <string.h>
#include <json.h>

bool parseParametersJson(const char* json_path, TEST_PARAMETERS* parameters) {
  json_object* jso = NULL;
  jso = json_object_from_file(json_path);
  if (jso == NULL) {
    return false;
  }

  json_object_object_foreach(jso, key, value) {
    if (!strcmp(key, "power_mode")) {
      parameters->power_mode = std::string(json_object_get_string(value));
    } else if (!strcmp(key, "gamma_r")) {
      parameters->gamma_r = json_object_get_double(value);
    } else if (!strcmp(key, "gamma_g")) {
      parameters->gamma_g = json_object_get_double(value);
    } else if (!strcmp(key, "gamma_b")) {
      parameters->gamma_b = json_object_get_double(value);
    } else if (!strcmp(key, "brightness_r")) {
      parameters->brightness_r = json_object_get_int(value);
    } else if (!strcmp(key, "brightness_g")) {
      parameters->brightness_g = json_object_get_int(value);
    } else if (!strcmp(key, "brightness_b")) {
      parameters->brightness_b = json_object_get_int(value);
    } else if (!strcmp(key, "contrast_r")) {
      parameters->contrast_r = json_object_get_int(value);
    } else if (!strcmp(key, "contrast_g")) {
      parameters->contrast_g = json_object_get_int(value);
    } else if (!strcmp(key, "contrast_b")) {
      parameters->contrast_b = json_object_get_int(value);
    } else if (!strcmp(key, "broadcast_rgb")) {
      parameters->broadcast_rgb = std::string(json_object_get_string(value));
    } else if (!strcmp(key, "layers_parameters")) {
      struct array_list* array = json_object_get_array(value);
      int len = json_object_array_length(value);
      for (int i = 0; i < len; i++) {
        LAYER_PARAMETER layer_parameter;
        struct json_object* object =
            (struct json_object*)array_list_get_idx(array, i);
        json_object_object_foreach(object, layer_key, layer_value) {
          if (strcmp(layer_key, "type") == 0) {
            layer_parameter.type = (LAYER_TYPE)json_object_get_int(layer_value);
          } else if (strcmp(layer_key, "format") == 0) {
            layer_parameter.format =
                (LAYER_FORMAT)json_object_get_int(layer_value);
          } else if (strcmp(layer_key, "transform") == 0) {
            layer_parameter.transform =
                (LAYER_TRANSFORM)json_object_get_int(layer_value);
          } else if (strcmp(layer_key, "resource_path") == 0) {
            layer_parameter.resource_path =
                std::string(json_object_get_string(layer_value));
          } else if (strcmp(layer_key, "source") == 0) {
            json_object_object_foreach(layer_value, source_key, source_value) {
              if (strcmp(source_key, "width") == 0) {
                layer_parameter.source_width =
                    json_object_get_int(source_value);
              } else if (strcmp(source_key, "height") == 0) {
                layer_parameter.source_height =
                    json_object_get_int(source_value);
              } else if (strcmp(source_key, "crop") == 0) {
                json_object_object_foreach(source_value, crop_key, crop_value) {
                  if (strcmp(crop_key, "x") == 0) {
                    layer_parameter.source_crop_x =
                        json_object_get_int(crop_value);
                  } else if (strcmp(crop_key, "y") == 0) {
                    layer_parameter.source_crop_y =
                        json_object_get_int(crop_value);
                  } else if (strcmp(crop_key, "width") == 0) {
                    layer_parameter.source_crop_width =
                        json_object_get_int(crop_value);
                  } else if (strcmp(crop_key, "height") == 0) {
                    layer_parameter.source_crop_height =
                        json_object_get_int(crop_value);
                  }
                }
              }
            }
          } else if (strcmp(layer_key, "frame") == 0) {
            json_object_object_foreach(layer_value, frame_key, frame_value) {
              if (strcmp(frame_key, "x") == 0) {
                layer_parameter.frame_x = json_object_get_int(frame_value);
              } else if (strcmp(frame_key, "y") == 0) {
                layer_parameter.frame_y = json_object_get_int(frame_value);
              } else if (strcmp(frame_key, "width") == 0) {
                layer_parameter.frame_width = json_object_get_int(frame_value);
              } else if (strcmp(frame_key, "height") == 0) {
                layer_parameter.frame_height = json_object_get_int(frame_value);
              }
            }
          }
        }
        parameters->layers_parameters.push_back(layer_parameter);
      }
    }
  }

  return true;
}
