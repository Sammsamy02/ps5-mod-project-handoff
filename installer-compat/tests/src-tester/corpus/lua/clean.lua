local mod = {}
function mod.onLoad()
  local it = FindFirstOf("Item")
  while it do
    it = FindNextOf(it)
  end
end
return mod
