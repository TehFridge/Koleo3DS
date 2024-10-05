
TextDraw = {}

function TextDraw.GetWrappedText(text, font, width, scale)
    scale = scale or 1
    text = tostring(text)
    local font = font or love.graphics.getFont()
    local newString = ""
    local line = ""
    for word in text:gmatch("%S+") do
        local testLine = line .. word .. " "
        local testWidth = TextDraw.GetTextWidth(testLine, font, scale)
        if testWidth > width then
            newString = newString .. line .. "\n"
            line = word .. " "
        else
            line = testLine
        end
    end
    newString = newString .. line
    return newString
end
function TextDraw.DrawText(text, x, y, color, font, scale)
    local font = font or love.graphics.getFont()
    local scale = scale or 1
    love.graphics.setFont(font)
    love.graphics.setColor(color)
    love.graphics.print(text, x, y, 0, scale, scale)
end

function DrawRectangle(y, SCREEN_WIDTH, color, height, bg, x)
	if bg == false then
		if love._console == "3DS" then
			local subtraction = 8
			love.graphics.setColor(color)
			love.graphics.rectangle("fill", x, y - subtraction, SCREEN_WIDTH - 10, height, 5, 5, 12)
			love.graphics.setColor(0,0,0,1)
			love.graphics.rectangle("line", x, y - subtraction, SCREEN_WIDTH - 10, height, 5, 5, 12)
		elseif love._console == "Switch" then
			local subtraction = 12
			love.graphics.setColor(color)
			love.graphics.rectangle("fill", x, y - subtraction, SCREEN_WIDTH - 10, height, 5, 5, 12)
			love.graphics.setColor(0,0,0,1)
			love.graphics.rectangle("line", x, y - subtraction, SCREEN_WIDTH - 10, height, 5, 5, 12)
		end
	else
		love.graphics.setColor(color)
		love.graphics.rectangle("fill", 0, y, SCREEN_WIDTH, height, 0, 0, 0)
	end
end

function TextDraw.DrawTextCentered(text, x, y, color, font, scale)
    local width = TextDraw.GetTextWidth(text, font, scale)
    local height = TextDraw.GetTextHeight(text, font, scale)
    TextDraw.DrawText(text, x - width / 2, y - height / 2, color, font, scale)
end
function DrawKrynciol(x,y) --thx nawias za kryncioła
	love.graphics.arc("fill",x,y,50,0,love.timer.getTime() * 4 % 6.283)
end
function DrawImage(image, x, y)
	love.graphics.setColor(1,1,1,1)
	love.graphics.draw(image, x, y)
end

function DrawImageCentered(image, x, y, scale)
	local width = image:getWidth() * scale
    local height = image:getHeight() * scale
	love.graphics.setColor(1,1,1,1)
	love.graphics.draw(image, x - width / 2, y - height / 2, 0, scale, scale)
end


function TextDraw.GetTextWidth(text, font, scale)
    local font = font or love.graphics.getFont()
    local scale = scale or 1
    local width = font:getWidth(text)
    return width * scale
end

function TextDraw.GetTextHeight(text, font, scale)
    local font = font or love.graphics.getFont()
    local scale = scale or 1
    local height = font:getHeight(text)
    return height * scale
end

return TextDraw