CONFIRM_NO = 0
CONFIRM_YES = 1
CONFIRM_OK = 1
CONFIRM_TIMEOUT = 2

MALE = 0
FEMALE = 1

setstate = q.setstate
newstate = q.setstate

function minmax(min_val, val, max_val)
    return val < min_val and min_val or val > max_val and max_val or val
end

function trim(s)
    return (string.gsub(s, "^%s*(.-)%s*$", "%1"))
end

function color(r, g, b)
    return "[COLOR r;"..r.."|g;"..g.."|b;"..b.."]"
end

function item_name(vnum)
    return "[ITEM value;"..vnum.."]"
end

function mob_name(vnum)
    return "[MOB value;"..vnum.."]"
end
