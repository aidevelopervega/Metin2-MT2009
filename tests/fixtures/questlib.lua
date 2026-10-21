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

function select(...)
    return q.yield('select', table.pack(...))
end

function select_table(tbl)
    return q.yield('select', tbl)
end

function wait()
    return q.yield('wait')
end

function input()
    return q.yield('input')
end

function confirm(vid, msg, timeout)
    return q.yield('confirm', vid, msg, timeout)
end

function select_item()
    return q.yield('select_item')
end
