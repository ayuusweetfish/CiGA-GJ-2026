local draw = require 'utils/draw'
local audio = require 'audio'

local chain = {}

return function ()
  local s = {}
  local W, H = W, H

  local i = draw.loadx('chain/a_2848.jpg')

  s.press = function (x, y)
  end

  s.hover = function (x, y)
  end

  s.move = function (x, y)
  end

  s.release = function (x, y)
  end

  s.update = function ()
  end

  s.draw = function ()
    love.graphics.clear(0, 0, 0)
    love.graphics.setColor(1, 1, 1)
    draw.img(i, W/2, H/2)
  end

  s.destroy = function ()
  end

  return s
end
