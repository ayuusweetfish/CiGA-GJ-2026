local draw = require 'utils/draw'
local button = require 'utils/button'
local audio = require 'audio'
local window_frame = require 'window_frame'

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

  ------------ Chain ------------

  local cur_at = 'a_lang.jpg'
  local prev_at = 'a_7312.jpg'
  -- Needs a correct label list to handle startup requirement
  local correct_labels = {['point array'] = true}

  ------------ Image display ------------

  -- Top-left corner size
  local img_x0, img_y0 = W * 0.5, 29
  -- Region size
  local img_rw, img_rh = W * 0.5 - 4, math.floor(H * 0.75)

  local img_cx, img_cy = img_x0 + img_rw / 2, img_y0 + img_rh / 2

  local cur_img, img_w, img_h
  local prev_img, prev_img_w, prev_img_h

  local push_cur_img = function (node_name)
    if prev_img ~= nil then
      draw.unload(prev_img)
    end
    prev_img = cur_img
    prev_img_w = img_w
    prev_img_h = img_h

    cur_img = draw.loadx('chain/' .. node_name)
    img_w, img_h = draw.get(cur_img):getDimensions()
    local scale = math.min(img_rw / img_w, img_rh / img_h)
    img_w = img_w * scale
    img_h = img_h * scale
  end
  push_cur_img(prev_at)
  push_cur_img(cur_at)

  ------------ State ------------

  local health = 480
  local correct_bonus = 240
  local incorrect_penalty = 120

  local since_reveal = -1
  local reveal_cur_label = nil
  local reveal_prev_labels = nil
  local reveal_due_to_currect = false

  local set_reveal_label = function (l)
    reveal_cur_label = l
    reveal_prev_labels = {}
    for j = 1, #chain[prev_at].labels do
      local lp = chain[prev_at].labels[j]
      if lp[1] == l[1] then
        reveal_prev_labels[#reveal_prev_labels + 1] = lp
      end
    end
  end

  local since_incorrect = -1
  local incorrect_x, incorrect_y

  local incorrect_count = 0

  ------------ Widgets ------------

  local btn_flip = button(draw.get('button_ord'), function ()
  end, draw.get('button_press'))
  btn_flip.x = math.floor(W * 0.08)
  btn_flip.y = math.floor(H * 0.75)

  ------------ Pointer events ------------

  local pt_on_img = function (x, y)
    x = 0.5 + (x - img_cx) / img_w
    y = 0.5 + (y - img_cy) / img_h
    if x < 0 or x > 1 or y < 0 or y > 1 then
      return nil, nil
    else
      return x, y
    end
  end

  local is_press_started_on_img = false

  s.press = function (x, y)
    if btn_flip.press(x, y) then return true end

    if since_reveal >= 0 or since_incorrect >= 0 then return true end
    is_press_started_on_img = (pt_on_img(x, y) ~= nil)
    return is_press_started_on_img
  end

  s.hover = function (x, y)
  end

  s.move = function (x, y)
    if btn_flip.move(x, y) then return true end
  end

  s.release = function (x, y)
    if btn_flip.release(x, y) then return true end

    if not is_press_started_on_img then return end
    x, y = pt_on_img(x, y)
    if not x then return end

    -- Check whether matches a label region
    local correct = false
    for i = 1, #chain[cur_at].labels do
      local label_text, x1, y1, x2, y2 = unpack(chain[cur_at].labels[i])
      local w, h = (x2 - x1) * img_w, (y2 - y1) * img_h
      local tol_x = math.max(4, (36 - w) / 2) / img_w
      local tol_y = math.max(4, (36 - h) / 2) / img_h
      if x >= x1 - tol_x and x <= x2 + tol_x and
         y >= y1 - tol_y and y <= y2 + tol_y
      then
        -- Is correct?
        if correct_labels[label_text] then
          -- Yes!
          set_reveal_label(chain[cur_at].labels[i])
          since_reveal = 0
          reveal_due_to_currect = true
          correct_bonus = 40 + math.max(0, math.min(800, 960 - health)) / 4
          health = health + correct_bonus
          if health >= 960 then
            -- TODO: Win?
          end
          correct = true
          break
        end
      end
    end

    if not correct then
      since_incorrect = 0
      incorrect_x, incorrect_y = x, y
      incorrect_count = incorrect_count + 1
      if incorrect_count >= 2 then
        -- Reveal answer on the second incorrect attempt
        incorrect_count = 0
        for i = 1, #chain[cur_at].labels do
          local label_text, x1, y1, x2, y2 = unpack(chain[cur_at].labels[i])
          if correct_labels[label_text] then
            set_reveal_label(chain[cur_at].labels[i])
          end
        end
        since_reveal = 0
        reveal_due_to_currect = false
      end
      health = health - incorrect_penalty
      if health < 0 then
        -- TODO: Lose?
      end
    end

    is_press_started_on_img = false
    return true
  end

  ------------ Update ------------

  s.update = function ()
    btn_flip.update()
    if since_incorrect >= 0 then
      since_incorrect = since_incorrect + 1
      if since_incorrect >= 240 then
        since_incorrect = -1
      end
    end
    if since_reveal >= 0 and (since_incorrect < 0 or since_incorrect > 120) then
      since_reveal = since_reveal + 1
      if since_reveal >= 600 then
        -- Find a new target
        local out_links = chain[cur_at].links
        if #out_links == 0 then
          return  -- XXX: This should not happen! Debug use only
        end
        local go_to = out_links[love.math.random(#out_links)]
        correct_labels = {}
        local cur_labels = {}
        for i = 1, #chain[cur_at].labels do
          local label_name = chain[cur_at].labels[i][1]
          cur_labels[label_name] = true
        end
        for i = 1, #chain[go_to].labels do
          local label_name = chain[go_to].labels[i][1]
          if cur_labels[label_name] then
            correct_labels[label_name] = true
          end
        end
        prev_at, cur_at = cur_at, go_to
        push_cur_img(cur_at)
        incorrect_count = 0
        since_reveal = -1
      end
    end

    if btn_flip.inside then
      health = health - 0.5
      if health <= 0 then
        health = 0
      end
    end
  end

  ------------ Draw ------------

  local font = _G['global_font'](15)
  local label_font = _G['global_font'](15)

  local draw_label = function (half, label, x1, y1, x2, y2)
    love.graphics.setColor(1, 0.5, 0.4)
    love.graphics.setLineWidth(W * 0.002)
    local img_cx = img_cx
    if half == 1 then img_cx = W - img_cx end
    local img_w, img_h = img_w, img_h
    if half == 1 then img_w, img_h = prev_img_w, prev_img_h end
    x1 = img_cx + img_w * (x1 - 0.5)
    x2 = img_cx + img_w * (x2 - 0.5)
    y1 = img_cy + img_h * (y1 - 0.5)
    y2 = img_cy + img_h * (y2 - 0.5)
    love.graphics.rectangle('line', x1, y1, x2 - x1, y2 - y1)
    local t = love.graphics.newText(label_font, label)
    love.graphics.rectangle('fill', x1, y1 - t:getHeight(), t:getWidth(), t:getHeight())
    love.graphics.setColor(1, 1, 1)
    love.graphics.draw(t, x1, y1 - t:getHeight())
  end

  local t1 = love.graphics.newText(font, '验证您是人类：')
  local t2 = love.graphics.newText(font, '请在这张图片中找出上一张图片内\n出现过的同类物体')

  local btn_flip_t = love.graphics.newText(font, '上一张')

  s.draw = function ()
    love.graphics.clear(0, 0, 0.5)
    love.graphics.setColor(1, 1, 1)

    love.graphics.push()
    if since_incorrect >= 0 and since_incorrect < 120 then
      local t = math.floor(since_incorrect / 10) * 10
      local x = love.math.noise(234, t * 0.876)
      local y = love.math.noise(t * 0.876, 123)
      local d = 8 * math.exp(-t * 0.02)
      love.graphics.translate(
        math.floor(x * d - d / 2),
        math.floor(y * d - d / 2)
      )
    end

    window_frame.draw_window('Anchor Verification System', W, H, W * 0.5, H * 0.5)
    window_frame.draw_lupa(W, H, W * 0.5, H * 0.5,
      2, 24, W - 2, img_y0 + img_rh + 1 + (img_y0 - 24))

    love.graphics.setColor(1, 1, 1)
    love.graphics.rectangle('fill', img_x0, img_y0, img_rw, img_rh)

    love.graphics.setColor(1, 1, 1)
    draw.img(cur_img, img_cx, img_cy, img_w)

    if (since_reveal >= 0 and (since_incorrect < 0 or since_incorrect >= 120))
      or btn_flip.inside
    then
      love.graphics.setColor(1, 1, 1, 0.5)
      love.graphics.rectangle('fill', W - (img_x0 + img_rw), img_y0, img_rw, img_rh)
      love.graphics.setColor(1, 1, 1)
      draw.img(prev_img, W - img_cx, img_cy, prev_img_w)
      if since_reveal >= 0 then
        draw_label(0, unpack(reveal_cur_label))
        for i = 1, #reveal_prev_labels do
          draw_label(1, unpack(reveal_prev_labels[i]))
        end
      end
    else
      love.graphics.setColor(0, 0, 0)
      local title_y = 24 + math.floor(H * 0.1)
      draw(t1, 14, title_y + 0, nil, nil, 0, 0)
      draw(t2, 14, title_y + 24, nil, nil, 0, 0)
    end

    if _G['jam_debug'] then
      for i = 1, #chain[cur_at].labels do
        local label, x1, y1, x2, y2 = unpack(chain[cur_at].labels[i])
        draw_label(0, label, x1, y1, x2, y2)
      end
    end

    if since_incorrect >= 0 then
      local x = img_cx + img_w * (incorrect_x - 0.5)
      local y = img_cy + img_h * (incorrect_y - 0.5)
      love.graphics.setColor(1, 1, 1)
      draw.img('blossom', x, y, 36)
    end

    if since_reveal < 0 then
      love.graphics.setColor(1, 1, 1)
      btn_flip.draw()
      love.graphics.setColor(0, 0, 0)
      draw(btn_flip_t,
        btn_flip.x + (btn_flip.inside and -1 or 0),
        btn_flip.y + (btn_flip.inside and 1 or 0),
        nil, nil, 0.5, 0.5)
    end

    local health_bar_start = 4
    local health_bar_end = W - 4
    local health_bar_t = health / 960
    if since_reveal >= 0 and reveal_due_to_currect then
      local t = math.min(1, since_reveal / 240)
      t = (1 - t) * math.exp(-4 * t)
      health_bar_t = health_bar_t - correct_bonus / 960 * t
    end
    if since_incorrect >= 0 then
      local t = math.min(1, since_incorrect / 120)
      t = (1 - t) * math.exp(-4 * t)
      health_bar_t = health_bar_t + incorrect_penalty / 960 * t
    end
    health_bar_t = math.max(0, math.min(1, health_bar_t))
    love.graphics.setColor(0.2, 0.3, 0.2)
    love.graphics.rectangle('fill',
      health_bar_start,
      math.floor(H * 0.875),
      math.floor((health_bar_end - health_bar_start) * health_bar_t + 0.5),
      math.floor(H * 0.02)
    )

    love.graphics.pop()
  end

  s.destroy = function ()
    draw.unload(prev_img)
    draw.unload(cur_img)
  end

  return s
end
