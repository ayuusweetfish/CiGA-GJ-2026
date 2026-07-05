local draw = require 'utils/draw'

local window_titles = {}

local draw_window = function (title, w, h, cx, cy)
  draw.img('window', cx, cy, w, h)
  draw.img('window_logo', cx - w/2 + 4, cy - h/2 + 5, nil, nil, 0, 0)
  local title_text = window_titles[title]
  if not title_text then
    title_text = window_title or
      love.graphics.newText(_G['global_font'](15), title)
    window_titles[title] = title_text
  end
  love.graphics.setColor(0.71, 0.38, 0.33)
  draw(title_text, cx - w/2 + 25, cy - h/2 + 20, nil, nil, 0, 1)
  love.graphics.setColor(0, 0, 0)
  draw(title_text, cx - w/2 + 26, cy - h/2 + 20, nil, nil, 0, 1)
end

local draw_lupa = function (w, h, cx, cy, x1, y1, x2, y2)
  local x0, y0 = cx - w / 2, cy - h / 2
  x1, y1 = x1 + x0, y1 + y0
  x2, y2 = x2 + x0, y2 + y0
  love.graphics.setColor(0.52, 0.53, 0.58)
  love.graphics.rectangle('fill', x1, y1, x2 - x1, 1)
  love.graphics.rectangle('fill', x2 - 1, y1, 1, y2 - y1)
  love.graphics.setColor(0.87, 0.89, 0.92)
  love.graphics.rectangle('fill', x1, y1, 1, y2 - y1)
  love.graphics.rectangle('fill', x1, y2 - 1, x2 - x1, 1)
end

return {
  draw_window = draw_window,
  draw_lupa = draw_lupa,
}
