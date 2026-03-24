local Glow = {}

-- ------------------------------------------------------------
-- Helpers
-- ------------------------------------------------------------

local function clamp01(x)
    if x < 0 then return 0 end
    if x > 1 then return 1 end
    return x
end

local function applyRegions(regions, values)
    for i = 1, #regions do
        setEffectRegionOpacity(regions[i], values[i] or values[1])
    end
end

-- ------------------------------------------------------------
-- FIRE LIGHT (oil lamps, torches)
-- ------------------------------------------------------------

function Glow.startFire(cfg)
    local regions = cfg.regions
    local base = cfg.base

    local baseA = base[1]
    local baseB = base[2] or baseA

    local targetA = baseA
    local targetB = baseB

    while true do
        -- occasional drift (flame instability)
        if math.random(1, 100) <= 18 then
            targetA = math.random(75, 95) / 100
            targetB = math.random(20, 55) / 100
        end

        -- drift toward target
        baseA = baseA + (targetA - baseA) * 0.18
        baseB = baseB + (targetB - baseB) * 0.18

        -- fast flicker on top
        local flickerA = (math.random(-8, 8)) / 100
        local flickerB = (math.random(-12, 12)) / 800

        local a = clamp01(baseA + flickerA)
        local b = clamp01(baseB + flickerB)

        applyRegions(regions, { a, b })

        delay(math.random(40, 120))
    end
end

-- ------------------------------------------------------------
-- ELECTRIC LIGHT (stable, slight shimmer)
-- ------------------------------------------------------------

function Glow.startElectric(cfg)
    local regions = cfg.regions
    local base = cfg.base

    local baseA = base[1]
    local baseB = base[2] or baseA

    local targetA = baseA
    local targetB = baseB

    while true do
        -- rare small fluctuation
        if math.random(1, 100) <= 5 then
            targetA = math.random(62, 68) / 100
            targetB = math.random(42, 48) / 100
        end

        -- very slow drift
        baseA = baseA + (targetA - baseA) * 0.03
        baseB = baseB + (targetB - baseB) * 0.03

        -- tiny shimmer
        local shimmerA = (math.random(-2, 2)) / 200
        local shimmerB = (math.random(-2, 2)) / 300

        local a = clamp01(baseA + shimmerA)
        local b = clamp01(baseB + shimmerB)

        applyRegions(regions, { a, b })

        delay(math.random(80, 160))
    end
end

-- ------------------------------------------------------------
-- WINDOW LIGHT / SOFT AMBIENT (very subtle)
-- ------------------------------------------------------------

function Glow.startWindowLight(cfg)
    local regions = cfg.regions
    local base = cfg.base

    local baseA = base[1]
    local baseB = base[2] or baseA

    local cycleDuration = math.random(18000, 26000)
    local cycleStart = os.clock() * 1000.0

    while true do
        local now = os.clock() * 1000.0
        local t = (now - cycleStart) / cycleDuration

        if t >= 1.0 then
            cycleStart = now
            cycleDuration = math.random(18000, 26000)
            t = 0.0
        end

        -- smooth 0..1..0 wave
        local wave = 0.5 - 0.5 * math.cos(t * math.pi * 2.0)

        -- VERY subtle modulation
        local a = baseA + wave * 0.02
        local b = baseB + wave * 0.01

        applyRegions(regions, {
            clamp01(a),
            clamp01(b)
        })

        delay(200)
    end
end

return Glow
