-- bench.lua
local obj = TestBase.new()
local start = os.clock()
local d = 1000000
for i = 1, d do
    obj:SetId(i)
end
local elapsed = os.clock() - start
print(string.format("%d call: %.3f s one: %.1f ns",d, elapsed, elapsed * 1e9 / 1e6))
