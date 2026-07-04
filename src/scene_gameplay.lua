local draw = require 'utils/draw'
local audio = require 'audio'

local parse_chain = function (box_csv, links_csv)
  local chain = {}

  local function split_fields(line)
    local fields = {}
    for field in line:gmatch('[^,]+') do
      fields[#fields + 1] = field
    end
    return fields
  end

  -- Bounding boxes
  for line in box_csv:gmatch('[^\n]+') do if line:byte(1, 1) ~= 35 then
    local fields = split_fields(line)
    if #fields >= 6 then
      local imgid = fields[1]
      local x1, y1 = tonumber(fields[2]), tonumber(fields[3])
      local x2, y2 = tonumber(fields[4]), tonumber(fields[5])
      local label = fields[6]
      if not chain[imgid] then
        chain[imgid] = {labels = {}, links = {}}
      end
      table.insert(chain[imgid].labels, {label, x1, y1, x2, y2})
    end
  end end

  -- Links
  for line in links_csv:gmatch('[^\n]+') do if line:byte(1, 1) ~= 35 then
    local fields = split_fields(line)
    if #fields >= 2 then
      local source = fields[1]
      local target = fields[2]
      if not chain[source] then
        chain[source] = {labels = {}, links = {}}
      end
      table.insert(chain[source].links, target)
    end
  end end

  return chain
end

local chain = parse_chain(
  love.filesystem.read('chain/_box.csv'),
  love.filesystem.read('chain/_links.csv')
)

return function ()
  local s = {}
  local W, H = W, H

  local img_cx, img_cy = W / 2, H / 2

  local cur_at = 'oid_e70d2d777077e8a3.jpg'
  local cur_img
  local img_w, img_h, img_scale

  local update_img = function ()
    cur_img = draw.loadx('chain/' .. cur_at)
    img_w, img_h = draw.get(cur_img):getDimensions()
    img_scale = math.min(W * 0.8 / img_w, H * 0.8 / img_h)
  end
  update_img()

  s.press = function (x, y)
  end

  s.hover = function (x, y)
  end

  s.move = function (x, y)
  end

  s.release = function (x, y)
    -- Find a new target
    local out_links = chain[cur_at].links
    if #out_links == 0 then
      return  -- XXX: This should not happen! Debug use only
    end
    local go_to = out_links[love.math.random(#out_links)]
    draw.unload(cur_img)
    cur_at = go_to
    update_img()
  end

  s.update = function ()
  end

  local label_font = _G['global_font'](32)

  local draw_label = function (label, x1, y1, x2, y2)
    love.graphics.setColor(1, 0.5, 0.4)
    love.graphics.setLineWidth(W * 0.002)
    x1 = img_cx + img_w * img_scale * (x1 - 0.5)
    x2 = img_cx + img_w * img_scale * (x2 - 0.5)
    y1 = img_cy + img_h * img_scale * (y1 - 0.5)
    y2 = img_cy + img_h * img_scale * (y2 - 0.5)
    love.graphics.rectangle('line', x1, y1, x2 - x1, y2 - y1)
    local t = love.graphics.newText(label_font, label)
    love.graphics.rectangle('fill', x1, y1 - t:getHeight(), t:getWidth(), t:getHeight())
    love.graphics.setColor(1, 1, 1)
    love.graphics.draw(t, x1, y1 - t:getHeight())
  end

  s.draw = function ()
    love.graphics.clear(0, 0, 0)
    love.graphics.setColor(1, 1, 1)
    draw.img(cur_img, W/2, H/2, img_w * img_scale)
    for i = 1, #chain[cur_at].labels do
      local label, x1, y1, x2, y2 = unpack(chain[cur_at].labels[i])
      draw_label(label, x1, y1, x2, y2)
    end
  end

  s.destroy = function ()
  end

  return s
end
