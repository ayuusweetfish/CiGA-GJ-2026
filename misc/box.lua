local f = function (W, H, w, h, x, y)
  return x / W, y / H, (x + w) / W, (y + h) / H
end

print('Enter: W, H, w, h, x, y')
while true do
  local line = io.read('*l')
  if not line then break end
  local fields = {}
  for w in line:gmatch('%S+') do
    fields[#fields + 1] = tonumber(w)
  end
  if #fields == 0 then  -- No-op
  elseif #fields ~= 6 then print('Invalid')
  else
    print(string.format('%.3f,%.3f,%.3f,%.3f',
      f((table.unpack or unpack)(fields))))
  end
end
