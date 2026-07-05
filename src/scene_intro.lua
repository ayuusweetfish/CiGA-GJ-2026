local draw = require 'utils/draw'
local button = require 'utils/button'
local audio = require 'audio'
local window_frame = require 'window_frame'

return function ()
  local s = {}
  local W, H = W, H
  local font = _G['global_font'](15)

  local start_at = 'a_7312.jpg'
  local start_img = draw.loadx('chain/' .. start_at)

  local img_rcx, img_rcy = W * 0.5, H * 0.516
  local img_rw, img_rh = W * 0.55, H * 0.32
  local img_w, img_h = draw.get(start_img):getDimensions()
  local scale = math.min(img_rw / img_w, img_rh / img_h)
  img_w = math.floor(img_w * scale / 2 + 0.5) * 2   -- Keep even
  img_h = math.floor(img_h * scale / 2 + 0.5) * 2

  local t1 = love.graphics.newText(font, '请在即将看到的图片中找到以下物体。')

  local btn_confirm = button(draw.get('button_ord'), function ()
    replaceScene(_G['scene_gameplay'](start_at), _G['transitions']['hardcut']())
  end, draw.get('button_press'))
  btn_confirm.x = math.floor(W * 0.5)
  btn_confirm.y = math.floor(H * 0.75)

  s.press = function (x, y)
    if btn_confirm.press(x, y) then return true end
  end

  s.hover = function (x, y)
  end

  s.move = function (x, y)
    if btn_confirm.move(x, y) then return true end
  end

  s.release = function (x, y)
    if btn_confirm.release(x, y) then return true end
  end

  s.update = function ()
    btn_confirm.update()
  end

  local btn_confirm_t = love.graphics.newText(font, '确认')

  s.draw = function ()
    love.graphics.clear(0, 0, 0.5)
    love.graphics.setColor(1, 1, 1)

    local cx, cy = W * 0.5, H * 0.5
    local window_w = math.floor(W * 0.625)
    local window_h = math.floor(H * 0.625)

    window_frame.draw_window('Anchor Verification System',
      window_w, window_h, cx, cy)
    window_frame.draw_lupa(
      window_w, window_h, cx, cy,
      2, 24, window_w - 2, window_h - 2)

    love.graphics.setColor(0, 0, 0)
    draw(t1, cx, cy - window_h / 2 + 32, nil, nil, 0.5, 0)

    love.graphics.setColor(1, 1, 1)
    draw.img(start_img, img_rcx, img_rcy, img_w, img_h)

    love.graphics.setColor(1, 1, 1)
    btn_confirm.draw()
    love.graphics.setColor(0, 0, 0)
    draw(btn_confirm_t,
      btn_confirm.x + (btn_confirm.inside and -1 or 0),
      btn_confirm.y + (btn_confirm.inside and 1 or 0),
      nil, nil, 0.5, 0.5)
  end

  s.destroy = function ()
    -- Initial image will be unloaded by the gameplay scene
  end

  return s
end
