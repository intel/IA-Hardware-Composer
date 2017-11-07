/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "disjoint_layers.h"
#include <assert.h>
#include <stdint.h>
#include <algorithm>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <utility>
#include <vector>
#include "hwctrace.h"

namespace hwcomposer {

enum EventType { START, END };

struct YPOI {
  EventType type;
  uint64_t y;
  uint64_t rect_id;

  bool operator<(const YPOI &rhs) const {
    if (y == rhs.y)
      return rect_id < rhs.rect_id;
    else
      return (y < rhs.y);
  }
};

// Any region will have start X and set of Y coordinates.
struct Region {
  uint64_t sx;
  std::set<YPOI> y_points;
  RectIDs rect_ids;
};

// POI is the point of interest while traversing through x coordinates
struct POI {
  EventType type;
  uint64_t rect_id;
  uint64_t x;
  uint64_t top_y;
  uint64_t bot_y;

  bool operator<(const POI &rhs) const {
    return (x <= rhs.x);
  }
};

// This function will take active region and right x
// For an active region there will be set of YPOI
// It will traverse through each y_poi and given out
// rectangle with rect_ids active at that time.
void GenerateOutLayers(Region *reg, uint64_t x,
                       std::vector<RectSet<int>> *out) {
  Rect<int> out_rect;
  out_rect.left = reg->sx;
  out_rect.right = x;
  RectIDs rect_ids;

  for (std::set<YPOI>::iterator y_poi_it = reg->y_points.begin();
       y_poi_it != reg->y_points.end(); y_poi_it++) {
    const YPOI &y_poi = *y_poi_it;
    // No need to check for start or end event
    // as rect_ids is empty
    if (rect_ids.isEmpty()) {
      out_rect.top = y_poi.y;
      rect_ids.add(y_poi.rect_id);
    } else {
      if (out_rect.top == static_cast<int>(y_poi.y)) {
        if (y_poi.type == START) {
          rect_ids.add(y_poi.rect_id);
        } else {
          rect_ids.subtract(y_poi.rect_id);
        }
        continue;
      }
      out_rect.bottom = y_poi.y;
      out->emplace_back(RectSet<int>(rect_ids, out_rect));
      out_rect.top = y_poi.y;
      if (y_poi.type == START) {
        rect_ids.add(y_poi.rect_id);
      } else {
        rect_ids.subtract(y_poi.rect_id);
      }
    }
  }
}

// This function will remove y coordinates corresponding to given rect_id
void RemoveYpois(Region *reg, uint64_t rect_id) {
  std::set<YPOI>::iterator top_it = reg->y_points.begin();
  while (top_it != reg->y_points.end()) {
    if ((*top_it).rect_id == rect_id) {
      reg->y_points.erase(top_it++);
    } else {
      top_it++;
    }
  }
}

bool compare_region(const Region *first, const Region *second) {
  uint64_t first_min_y = (*(first->y_points.begin())).y;
  uint64_t second_min_y = (*(second->y_points.begin())).y;
  return (first_min_y < second_min_y);
}

void get_draw_regions(const std::vector<Rect<int>> &in,
                      std::vector<RectSet<int>> *out) {
  if (in.size() > RectIDs::max_elements) {
    return;
  }

  // Set of all point of interests from input rectangles.
  std::set<POI> pois;
  std::list<Region *> imp_reg;
  std::list<Region> active_regions;

  // This loop will add all point of interests into pois.
  for (uint64_t i = 0; i < in.size(); i++) {
    const Rect<int> &rect = in[i];

    // Filter out empty or invalid rects.
    if (rect.left >= rect.right || rect.top >= rect.bottom)
      continue;

    POI poi;
    poi.rect_id = i;
    poi.x = rect.left;
    poi.top_y = rect.top;
    poi.bot_y = rect.bottom;
    poi.type = START;
    pois.insert(poi);

    poi.type = END;
    poi.x = rect.right;
    pois.insert(poi);
  }

  for (std::set<POI>::iterator it = pois.begin(); it != pois.end(); ++it) {
    const POI &poi = *it;
    // First rectangle has to be inserted into active region
    // This condition will be true if existing all active
    // regions are already copied to out.
    // If current poi is of type END there are no active regions,
    // then this poi might already covered in previous pass
    if (active_regions.size() == 0 && poi.type == START) {
      Region reg;
      reg.sx = poi.x;
      YPOI y_poi;

      y_poi.rect_id = poi.rect_id;
      y_poi.type = START;
      y_poi.y = poi.top_y;
      reg.y_points.insert(y_poi);

      y_poi.type = END;
      y_poi.y = poi.bot_y;
      reg.y_points.insert(y_poi);

      RectIDs rectIds;
      rectIds.add(poi.rect_id);
      reg.rect_ids = rectIds;
      active_regions.push_back(reg);
      continue;
    }

    // If active_regions in not empty, Check if current
    // poi y points fall in range of any existing
    // active_regions.
    // If yes, get that active region and do further processing
    // If No, create a new region and insert into active regions
    // If it is start event then there is possibility that multiple
    // active_regions get impacted.
    // If it is end event then one or none active_regions will get
    // impacted.
    bool found = false;
    imp_reg.clear();
    std::list<Region>::iterator it_reg = active_regions.begin();
    while (it_reg != active_regions.end()) {
      Region &cur_reg = *it_reg;
      uint64_t min_y = (*(cur_reg.y_points.begin())).y;
      uint64_t max_y = (*(cur_reg.y_points.rbegin())).y;
      // If bottom y is less than minimum y in region or top y is greater than
      // max y in region, then this region is not impacted by this rect
      if (poi.bot_y <= min_y || poi.top_y >= max_y) {
        it_reg++;
        continue;
      } else {
        found = true;
        // Found atleast one affected active region. If it is start event,
        // add rect_id to cur_reg.rect_ids, also top_y and bot_y to
        // cur_reg.y_points. if it is end event, remove rect_id from
        // cur_reg.rect_ids and also top_y and bot_y from cur_reg.y_points.
        // Also, if it is end event, check cur_reg.rect_ids is non empty,
        // if it is empty remove region from active_regions.
        // If it is start or end event, check next poi.x and see if it is same
        // and
        // those y coordinates fall in this region and it is END event, if yes
        // 1) remove that rect_id and y coordinates as well
        // 2)contine to check next poi.x until you find mismatch x.
        if (poi.x == cur_reg.sx) {
          if (poi.type == START) {
            cur_reg.rect_ids.add(poi.rect_id);
            imp_reg.push_back(&cur_reg);
          }

          it_reg++;
          continue;
        }
        if (poi.type == START) {
          GenerateOutLayers(&cur_reg, poi.x, out);
          cur_reg.sx = poi.x;
          cur_reg.rect_ids.add(poi.rect_id);
          imp_reg.push_back(&cur_reg);
          std::set<POI>::iterator next_poi_it = it;
          next_poi_it++;
          for (; next_poi_it != pois.end(); next_poi_it++) {
            const POI &next_poi = *next_poi_it;
            if (next_poi.x != poi.x) {
              break;
            } else {
              if (next_poi.bot_y <= min_y || next_poi.top_y >= max_y ||
                  next_poi.type == START) {
                continue;
              }
              cur_reg.rect_ids.subtract(next_poi.rect_id);
              RemoveYpois(&cur_reg, next_poi.rect_id);
            }
          }
          it_reg++;
        } else {
          GenerateOutLayers(&cur_reg, poi.x, out);
          RemoveYpois(&cur_reg, poi.rect_id);
          cur_reg.sx = poi.x;
          cur_reg.rect_ids.subtract(poi.rect_id);

          std::set<POI>::iterator next_poi_it = it;
          next_poi_it++;
          for (; next_poi_it != pois.end(); next_poi_it++) {
            const POI &next_poi = *next_poi_it;
            if (next_poi.x != poi.x) {
              break;
            } else {
              if (next_poi.bot_y <= min_y || next_poi.top_y >= max_y ||
                  next_poi.type == START) {
                continue;
              }
              cur_reg.rect_ids.subtract(next_poi.rect_id);
              RemoveYpois(&cur_reg, next_poi.rect_id);
            }
          }
          if (cur_reg.rect_ids.isEmpty()) {
            active_regions.erase(it_reg++);
          } else {
            it_reg++;
          }
        }
      }
    }
    // If no affected active region found, add new active region
    if (!found && poi.type == START) {
      Region reg;
      reg.sx = poi.x;
      YPOI y_poi;

      y_poi.rect_id = poi.rect_id;
      y_poi.type = START;
      y_poi.y = poi.top_y;
      reg.y_points.insert(y_poi);

      y_poi.type = END;
      y_poi.y = poi.bot_y;
      reg.y_points.insert(y_poi);

      RectIDs rectIds;
      rectIds.add(poi.rect_id);
      reg.rect_ids = rectIds;
      active_regions.push_back(reg);
    } else {
      if (imp_reg.size() > 1 && poi.type == START) {
        imp_reg.sort(compare_region);
        uint64_t cur_y = 0;
        for (std::list<Region *>::iterator cur_imp_reg_it = imp_reg.begin();
             cur_imp_reg_it != imp_reg.end(); cur_imp_reg_it++) {
          Region &cur_imp_reg = *(*cur_imp_reg_it);
          YPOI y_poi;
          y_poi.rect_id = poi.rect_id;
          y_poi.type = START;

          if (cur_y == 0) {
            y_poi.y = poi.top_y;
          } else {
            y_poi.y = cur_y;
          }
          // This is to split vertical
          // line into all impacted
          // regions.
          cur_imp_reg.y_points.insert(y_poi);
          // Take bottom of current region as start of next impacted region
          cur_y = (*(cur_imp_reg.y_points.rbegin())).y;
          std::list<Region *>::iterator next_imp_reg_it = cur_imp_reg_it;
          next_imp_reg_it++;
          if (next_imp_reg_it == imp_reg.end()) {
            // If there is an another
            // region which is impacted, no
            // need to add anything.
            // if there is no other active region left,
            // take bottom y and push into this active region
            y_poi.y = poi.bot_y;
          } else {
            y_poi.y = cur_y;
          }
          y_poi.type = END;
          cur_imp_reg.y_points.insert(y_poi);
        }
      } else if (imp_reg.size() == 1 && poi.type == START) {
        // Only one region got impacted add y coordinated to that region
        std::list<Region *>::iterator cur_imp_reg_it = imp_reg.begin();
        YPOI y_poi;
        y_poi.rect_id = poi.rect_id;
        y_poi.type = START;
        y_poi.y = poi.top_y;
        (*cur_imp_reg_it)->y_points.insert(y_poi);
        y_poi.type = END;
        y_poi.y = poi.bot_y;
        (*cur_imp_reg_it)->y_points.insert(y_poi);
      }
    }
  }
}

}  // namespace hwcomposer
