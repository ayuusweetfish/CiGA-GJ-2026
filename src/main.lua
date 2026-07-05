W = 640
H = 360

local isMobile = (love.system.getOS() == 'Android' or love.system.getOS() == 'iOS')
local isWeb = (love.system.getOS() == 'Web')

love.window.setMode(
  isWeb and W * 1 or W * 2,
  isWeb and H * 1 or H * 2,
  { fullscreen = false, highdpi = false }
)

love.graphics.setDefaultFilter('nearest', 'nearest')

local globalScale, Wx, Hx, offsX, offsY

local updateLogicalDimensions = function ()
  love.window.setTitle('我是人類')
  local wDev, hDev = love.graphics.getDimensions()
  globalScale = math.min(wDev / W, hDev / H)
  Wx = wDev / globalScale
  Hx = hDev / globalScale
  offsX = (Wx - W) / 2
  offsY = (Hx - H) / 2
end
updateLogicalDimensions()

-- Load font
local fontSizeFactory = function (path, preload)
  local font = {}
  if preload ~= nil then
    for i = 1, #preload do
      local size = preload[i]
      if path == nil then
        font[size] = love.graphics.newFont(size)
      else
        font[size] = love.graphics.newFont(path, size)
      end
    end
  end
  return function (size)
    if font[size] == nil then
      if path == nil then
        font[size] = love.graphics.newFont(size)
      else
        font[size] = love.graphics.newFont(path, size)
      end
    end
    return font[size]
  end
end
_G['global_font'] = fontSizeFactory('fnt/WenQuanYi_Bitmap_Song_14px.ttf', {15, 30})
love.graphics.setFont(_G['global_font'](15))

_G['scene_loading'] = require 'scene_loading'

local audio = require 'audio'
local bgm, bgm_update

local curScene = scene_loading(function ()
  bgm, bgm_update = audio.loop(
    nil, 0,
    'aud/bg_饭_0523a.ogg', (36 * 3) / (184.5 / 60),
    1600 * 4)
  bgm:setVolume(1)
  bgm:stop()
  _G['scene_intro'] = require 'scene_intro'
  _G['scene_gameplay'] = require 'scene_gameplay'
end)
local lastScene = nil
local transitionTimer = 0
local currentTransition = nil
local transitions = {}
_G['transitions'] = transitions

_G['replaceScene'] = function (newScene, transition)
  lastScene = curScene
  curScene = newScene
  transitionTimer = 0
  currentTransition = transition or transitions['fade'](0.9, 0.9, 0.9)
end

local mouseScene = nil
-- XXX: Monkey patch! (cursor)
local mouseX, mouseY = 0, 0
local since_click = -1

local isPaused = false

function love.mousepressed(x, y, button, istouch, presses)
  if button ~= 1 then return end
  if lastScene ~= nil then return end
  mouseScene = curScene
  curScene.press((x - offsX) / globalScale, (y - offsY) / globalScale)
end
function love.mousemoved(x, y, button, istouch)
  mouseX, mouseY = (x - offsX) / globalScale, (y - offsY) / globalScale
  curScene.hover((x - offsX) / globalScale, (y - offsY) / globalScale)
  if mouseScene ~= curScene then return end
  curScene.move((x - offsX) / globalScale, (y - offsY) / globalScale)
end
function love.mousereleased(x, y, button, istouch, presses)
  if button ~= 1 then return end
  since_click = 0
  if mouseScene ~= curScene then return end
  curScene.release((x - offsX) / globalScale, (y - offsY) / globalScale)
  mouseScene = nil
end

local isLCmdDown, isRCmdDown = false, false
local keyLCmd, keyRCmd =
  unpack(love.system.getOS() == 'OS X' and {'lgui', 'rgui'} or {'lctrl', 'rctrl'})
function love.keypressed(key)
  if key == 'lshift' then
    if not isMobile and not isWeb then
      love.window.setFullscreen(not love.window.getFullscreen())
      updateLogicalDimensions()
    end
  elseif key == 'space' then
    -- isPaused = not isPaused
  elseif key == keyLCmd then isLCmdDown = true
  elseif key == keyRCmd then isRCmdDown = true
  elseif key == 'q' and (isLCmdDown or isRCmdDown) then
    love.event.quit()
  elseif curScene.key ~= nil then
    curScene.key(key)
  end
end
function love.keyreleased(key)
  if key == keyLCmd then isLCmdDown = false
  elseif key == keyRCmd then isRCmdDown = false
  elseif curScene.keyrel ~= nil then
    curScene.keyrel(key)
  end
end

local T = 0
local timeStep = 1 / 240

local sinceAudioUpdate = 0

function love.update(dt)
  if isPaused then return end
  T = T + dt
  local count = 0
  while T > timeStep and count < 16 do
    T = T - timeStep
    count = count + 1
    if lastScene ~= nil then
      lastScene:update()
      transitionTimer = transitionTimer + 1
    end
    curScene:update()

    -- XXX: Monkey patch! (cursor)
    if since_click >= 0 then
      since_click = since_click + 1
      if since_click >= 8 * 20 then since_click = -1 end
    end
  end

  sinceAudioUpdate = sinceAudioUpdate + dt
  if sinceAudioUpdate >= 0.5 then
    sinceAudioUpdate = sinceAudioUpdate - 0.5
    -- if bgm_update then bgm_update() end
  end
  audio.sfx_update(dt)
end

transitions['hardcut'] = function ()
  return {
    dur = 0,
    draw = function (x)
      curScene:draw()
    end
  }
end
transitions['fade'] = function (r, g, b)
  return {
    dur = 120,
    draw = function (x)
      local opacity = 0
      if x < 0.5 then
        lastScene:draw()
        opacity = x * 2
      else
        curScene:draw()
        opacity = 2 - x * 2
      end
      love.graphics.setColor(r, g, b, opacity)
      love.graphics.rectangle('fill', -offsX, -offsY, Wx, Hx)
    end
  }
end

-- XXX: Monkey patch! (cursor)
local draw = require 'utils/draw'

function love.draw()
  love.graphics.scale(globalScale)
  love.graphics.setColor(1, 1, 1)
  love.graphics.push()
  love.graphics.translate(offsX, offsY)
  if lastScene ~= nil then
    local x = transitionTimer / currentTransition.dur
    currentTransition.draw(x)
    if x >= 1 then
      if lastScene.destroy then lastScene.destroy() end
      lastScene = nil
    end
  else
    curScene.draw()
  end
  local cursor_index = 1
  if since_click >= 0 then
    cursor_index = 1 + (math.floor(since_click / 20) + 1) % 4
  end
  local cursor = draw.get('cursor-' .. cursor_index)
  if cursor ~= nil then
    love.graphics.setColor(1, 1, 1)
    draw(cursor, math.floor(mouseX + 1.5), math.floor(mouseY + 1.5), 32, nil, 0.25, 0.25)
    love.graphics.setColor(0, 0, 0)
    draw(cursor, math.floor(mouseX + 0.5), math.floor(mouseY + 0.5), 32, nil, 0.25, 0.25)
  end
  love.mouse.setVisible(false)
  love.graphics.pop()
end
