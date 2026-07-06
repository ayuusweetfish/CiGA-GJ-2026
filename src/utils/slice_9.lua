local slice_9 = function (tex, borderWidth)
  local w, h = tex:getDimensions()
  local quads = {}
  local quadDimensions = {}
  local xs = {0, borderWidth, w - borderWidth, w}
  local ys = {0, borderWidth, h - borderWidth, h}
  for r = 1, 3 do
    for c = 1, 3 do
      local i = (r - 1) * 3 + c
      quads[i] = love.graphics.newQuad(
        xs[c], ys[r], xs[c + 1] - xs[c], ys[r + 1] - ys[r], w, h)
      quadDimensions[i] = {xs[c + 1] - xs[c], ys[r + 1] - ys[r]}
    end
  end

  local draw = function (x, y, w, h)
    local xs = {0, borderWidth, w - borderWidth, w}
    local ys = {0, borderWidth, h - borderWidth, h}
    for r = 1, 3 do
      for c = 1, 3 do
        local i = (r - 1) * 3 + c
        local qw, qh = unpack(quadDimensions[i])
        love.graphics.draw(tex, quads[i],
          x + xs[c], y + ys[r], 0,
          (xs[c + 1] - xs[c]) / qw,
          (ys[r + 1] - ys[r]) / qh)
      end
    end
  end

  return {
    draw = draw,
  }
end

return slice_9
