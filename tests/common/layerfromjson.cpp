#include "layerfromjson.h"

#include <string.h>
#include <json.h>

void simpleTest() {
  const char* path = "./test.json";
  LAYER_PARAMETERS parameters;
  bool ret = parseLayersFromJson(path, parameters);
}

bool parseLayersFromJson(const char* json_path, LAYER_PARAMETERS& parameters) {
  json_object* jso = NULL;
  jso = json_object_from_file(json_path);
  if (jso == NULL) {
    return false;
  }

  struct array_list* array = json_object_get_array(jso);
  int len = json_object_array_length(jso);
  for (int i = 0; i < len; i++) {
    LAYER_PARAMETER layer_paramter;
    struct json_object* object =
        (struct json_object*)array_list_get_idx(array, i);
    json_object_object_foreach(object, key, val) {
      if (strcmp(key, "type") == 0) {
        layer_paramter.type = (LAYER_TYPE)json_object_get_int(val);
      } else if (strcmp(key, "format") == 0) {
        layer_paramter.format = (LAYER_FORMAT)json_object_get_int(val);
      } else if (strcmp(key, "transform") == 0) {
        layer_paramter.transform = (LAYER_TRANSFORM)json_object_get_int(val);
      } else if (strcmp(key, "resourcePath") == 0) {
        layer_paramter.resource_path = std::string(json_object_get_string(val));
      } else if (strcmp(key, "source") == 0) {
        json_object_object_foreach(val, key1, attr) {
          if (strcmp(key1, "width") == 0) {
            layer_paramter.source_width = json_object_get_int(attr);
          } else if (strcmp(key1, "height") == 0) {
            layer_paramter.source_height = json_object_get_int(attr);
          } else if (strcmp(key1, "crop") == 0) {
            json_object_object_foreach(attr, key2, data) {
              if (strcmp(key2, "x") == 0) {
                layer_paramter.source_crop_x = json_object_get_int(data);
              } else if (strcmp(key2, "y") == 0) {
                layer_paramter.source_crop_y = json_object_get_int(data);
              } else if (strcmp(key2, "width") == 0) {
                layer_paramter.source_crop_width = json_object_get_int(data);
              } else if (strcmp(key2, "height") == 0) {
                layer_paramter.source_crop_height = json_object_get_int(data);
              }
            }
          }
        }
      } else if (strcmp(key, "frame") == 0) {
        json_object_object_foreach(val, key1, attr) {
          if (strcmp(key1, "x") == 0) {
            layer_paramter.frame_x = json_object_get_int(attr);
          } else if (strcmp(key1, "y") == 0) {
            layer_paramter.frame_y = json_object_get_int(attr);
          } else if (strcmp(key1, "width") == 0) {
            layer_paramter.frame_width = json_object_get_int(attr);
          } else if (strcmp(key1, "height") == 0) {
            layer_paramter.frame_height = json_object_get_int(attr);
          }
        }
      }
    }
    parameters.push_back(layer_paramter);
  }

  return true;
}
