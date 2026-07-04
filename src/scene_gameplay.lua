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
  for line in box_csv:gmatch('[^\n]+') do
    local fields = split_fields(line)
    if #fields >= 6 then
      local imgid = fields[1]
      local x1, x2 = tonumber(fields[2]), tonumber(fields[3])
      local y1, y2 = tonumber(fields[4]), tonumber(fields[5])
      local label = fields[6]
      if not chain[imgid] then
        chain[imgid] = {labels = {}, links = {}}
      end
      table.insert(chain[imgid].labels, {label, x1, x2, y1, y2})
    end
  end

  -- Links
  for line in links_csv:gmatch('[^\n]+') do
    local fields = split_fields(line)
    if #fields >= 2 then
      local source = fields[1]
      local target = fields[2]
      if not chain[source] then
        chain[source] = {labels = {}, links = {}}
      end
      table.insert(chain[source].links, target)
    end
  end

  return chain
end

local chain = parse_chain(
  love.filesystem.read('chain/_box.csv'),
  love.filesystem.read('chain/_links.csv')
)

return function ()
  local s = {}
  local W, H = W, H

  local cur_at = 'oid_e70d2d777077e8a3.jpg'
  local cur_img = draw.loadx('chain/' .. cur_at)

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
    cur_img = draw.loadx('chain/' .. cur_at)
  end

  s.update = function ()
  end

  s.draw = function ()
    love.graphics.clear(0, 0, 0)
    love.graphics.setColor(1, 1, 1)
    draw.img(cur_img, W/2, H/2)
  end

  s.destroy = function ()
  end

  return s
end
