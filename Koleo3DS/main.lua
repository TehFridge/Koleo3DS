-- main.lua
require "lib.text-draw"
local https = require("https")
local ltn12 = require("ltn12")
local json = require("lib.json")
--local logo = love.graphics.newImage("assets/logo.png")
if love._potion_version == nil then
	local nest = require("nest").init({ console = "3ds", scale = 1 })
	love._nest = true
    love._console_name = "3DS"
end
state = nil
if love._console == "3DS" then
	SCREEN_WIDTH = 400
	SCREEN_HEIGHT = 240
	LOGO_SCALE = 0.4
	LOGO_TEXT_OFFSET = 70
	HEADLINE_SCALE = 1.3
	PARAGRAPH_SCALE = 2
	BASE_PARAGRAPH_POS = 80
	ADD_PARAGRAPH_POS = 15
	BASE_GAP = 60
	RECTANGLE_HEIGHT = 56
	REC_GAP = 20
	player_speed = 25
elseif love._console == "Switch" then
	SCREEN_WIDTH = 1280
	SCREEN_HEIGHT = 720
	LOGO_SCALE = 0.9
	LOGO_TEXT_OFFSET = 130
	HEADLINE_SCALE = 2.5
	PARAGRAPH_SCALE = 2.2
	BASE_PARAGRAPH_POS = 130
	ADD_PARAGRAPH_POS = 30
	BASE_GAP = 100
	RECTANGLE_HEIGHT = 95
	REC_GAP = 8
end

function love.load()	
	--font = love.graphics.newFont("anyfont.otf", 8)
	hotstacje = {"wroclaw-glowny", "warszawa", "warszawa-centralna", "krakow", "krakow-glowny", "poznan-glowny", "katowice", "gdansk", "gdansk-glowny", "gdynia-glowna", "szczecin-glowny", "lodz-fabryczna", "warszawa-zachodnia", "warszawa-wschodnia", "bydgoszcz-glowna", "lublin-glowny", "rzeszow-glowny", "gdansk-wrzeszcz", "kolobrzeg", "opole-glowne", "kielce-glowne", "przemysl-glowny", "bialystok", "olsztyn", "olsztyn-glowny", "sopot", "czestochowa", "legnica", "lodz", "lodz-widzew", "torun", "torun-glowny", "zakopane", "lodz-kaliska", "gliwice", "bielsko-biala-glowna"}
	MOVE_PAGE = -20
	kurwacoto_jest = 1
	mode_sel = 1
    endstacjasel = 1
	stacjasel = 1
	additional_offset = 0
	state = "title_screen"
	rel_offset = 0
	selection = 1
	conurl = 1
	conselection = 1
	--refresh_data("https://konwenty-poludniowe.pl")
	stacjecache = love.filesystem.exists("stacje.txt")
	if stacjecache == false then
		refresh_data("https://koleo.pl/api/v2/main/stations")
		stacje = responded 
		love.filesystem.write("stacje.txt", body)
	else
		stacje = json.decode(love.filesystem.read("stacje.txt"))
	end
	REFRESHED = 0
end
function draw_top_screen(dt)
	DrawRectangle(0, SCREEN_WIDTH, {0.11, 0.05, 0.39, 1}, SCREEN_HEIGHT, true)
	if state == "lista_kon" then
		--love.graphics.print(tree, 10, 10)
		--love.graphics.print(header2, 10, 20)
		TextDraw.DrawTextCentered("Nadchodzące Wydarzenia", SCREEN_WIDTH/2, 40, {1, 1, 1, 1}, font, 2.1)
		if conselection == 1 then
			DrawRectangle(80, SCREEN_WIDTH, {0.35, 0.35, 0.35, 1}, 20.5, false, 5)
			TextDraw.DrawTextCentered(con1, SCREEN_WIDTH/2, 80, {0.28, 0.22, 0.34, 1}, font, 1.4)
		else
			DrawRectangle(80, SCREEN_WIDTH, {0.14, 0.14, 0.14, 1}, 20.5, false, 5)
			TextDraw.DrawTextCentered(con1, SCREEN_WIDTH/2, 80, {0.72, 0.63, 0.81, 1}, font, 1.4)
		end
		if conselection == 2 then
			DrawRectangle(100, SCREEN_WIDTH, {0.35, 0.35, 0.35, 1}, 20.5, false, 5)
			TextDraw.DrawTextCentered(con2, SCREEN_WIDTH/2, 100, {0.28, 0.22, 0.34, 1}, font, 1.4)
		else	
			DrawRectangle(100, SCREEN_WIDTH, {0.14, 0.14, 0.14, 1}, 20.5, false, 5)
			TextDraw.DrawTextCentered(con2, SCREEN_WIDTH/2, 100, {0.72, 0.63, 0.81, 1}, font, 1.4)
		end
		if conselection == 3 then
			DrawRectangle(120, SCREEN_WIDTH, {0.35, 0.35, 0.35, 1}, 20.5, false, 5)
			TextDraw.DrawTextCentered(con3, SCREEN_WIDTH/2, 120,{0.28, 0.22, 0.34, 1}, font, 1.4)
		else
			DrawRectangle(120, SCREEN_WIDTH, {0.14, 0.14, 0.14, 1}, 20.5, false, 5)
			TextDraw.DrawTextCentered(con3, SCREEN_WIDTH/2, 120, {0.72, 0.63, 0.81, 1}, font, 1.4)
		end
		if conselection == 4 then
			DrawRectangle(140, SCREEN_WIDTH, {0.35, 0.35, 0.35, 1}, 20.5, false, 5)
			TextDraw.DrawTextCentered(con4, SCREEN_WIDTH/2, 140, {0.28, 0.22, 0.34, 1}, font, 1.4)
		else
			DrawRectangle(140, SCREEN_WIDTH, {0.14, 0.14, 0.14, 1}, 20.5, false, 5)
		    TextDraw.DrawTextCentered(con4, SCREEN_WIDTH/2, 140, {0.72, 0.63, 0.81, 1}, font, 1.4)
		end
		if conselection == 5 then
			DrawRectangle(160, SCREEN_WIDTH, {0.35, 0.35, 0.35, 1}, 20.5, false, 5)
			TextDraw.DrawTextCentered(con5, SCREEN_WIDTH/2, 160, {0.28, 0.22, 0.34, 1}, font, 1.4)
		else
			DrawRectangle(160, SCREEN_WIDTH, {0.14, 0.14, 0.14, 1}, 20.5, false, 5)
			TextDraw.DrawTextCentered(con5, SCREEN_WIDTH/2, 160, {0.72, 0.63, 0.81, 1}, font, 1.4)
	    end
		if conselection == 6 then
			DrawRectangle(180, SCREEN_WIDTH, {0.35, 0.35, 0.35, 1}, 20.5, false, 5)
			TextDraw.DrawTextCentered(con6, SCREEN_WIDTH/2, 180, {0.28, 0.22, 0.34, 1}, font, 1.4)
		else
			DrawRectangle(180, SCREEN_WIDTH, {0.14, 0.14, 0.14, 1}, 20.5, false, 5)
		    TextDraw.DrawTextCentered(con6, SCREEN_WIDTH/2, 180, {0.72, 0.63, 0.81, 1}, font, 1.4)
		end
		if conselection == 7 then
			DrawRectangle(200, SCREEN_WIDTH, {0.35, 0.35, 0.35, 1}, 20.5, false, 5)
			TextDraw.DrawTextCentered(con7, SCREEN_WIDTH/2, 200, {0.28, 0.22, 0.34, 1}, font, 1.4)
		else
			DrawRectangle(200, SCREEN_WIDTH, {0.14, 0.14, 0.14, 1}, 20.5, false, 5)
			TextDraw.DrawTextCentered(con7, SCREEN_WIDTH/2, 200, {0.72, 0.63, 0.81, 1}, font, 1.4)
		end
		--love.graphics.print("Check console for parsed output.", 10, 10)
	elseif state == "title_screen" then
		--DrawImageCentered(logo, SCREEN_WIDTH/2, SCREEN_HEIGHT/2, LOGO_SCALE)
		TextDraw.DrawTextCentered("Koleo " .. love._console, SCREEN_WIDTH/2, (SCREEN_HEIGHT / 2) + LOGO_TEXT_OFFSET, {1, 1, 1, 1}, font, 2.1)
	elseif state == "loading" then
		DrawKrynciol(SCREEN_WIDTH/2,SCREEN_HEIGHT/2)
		--TextDraw.DrawTextCentered("Ładowanie...", SCREEN_WIDTH/2, SCREEN_HEIGHT / 2, {1, 1, 1, 1}, font, 2.1)
	elseif state == "main_strona" then
		--TextDraw.DrawTextCentered(parseurl(tree("h2")[1]:gettext()), SCREEN_WIDTH/2, 40, {1, 1, 1, 1}, font, 1) 
		TextDraw.DrawTextCentered(gdzieidokad, SCREEN_WIDTH/2, 20, {1, 1, 1, 1}, font, HEADLINE_SCALE)
		if kurwacoto_jest == 1 then 
			draw_widget(parsetime(pociagi.connections[1 + additional_offset].departure) .. " - " .. parsetime(pociagi.connections[1 + additional_offset].arrival) .. "   " .. cena1, BASE_PARAGRAPH_POS - REC_GAP, true)
		else
			draw_widget(parsetime(pociagi.connections[1 + additional_offset].departure) .. " - " .. parsetime(pociagi.connections[1 + additional_offset].arrival) .. "   " .. cena1, BASE_PARAGRAPH_POS - REC_GAP, false)
		end
		--TextDraw.DrawTextCentered(date1, SCREEN_WIDTH/2, BASE_PARAGRAPH_POS + ADD_PARAGRAPH_POS, {0.72, 0.63, 0.81, 1}, font, PARAGRAPH_SCALE)
		if kurwacoto_jest == 2 then
			draw_widget(parsetime(pociagi.connections[2 + additional_offset].departure) .. " - " .. parsetime(pociagi.connections[2 + additional_offset].arrival) .. "   " .. cena2, BASE_PARAGRAPH_POS + BASE_GAP - REC_GAP, true)
		else
			draw_widget(parsetime(pociagi.connections[2 + additional_offset].departure) .. " - " .. parsetime(pociagi.connections[2 + additional_offset].arrival) .. "   " .. cena2, BASE_PARAGRAPH_POS + BASE_GAP - REC_GAP, false)
		end
		--TextDraw.DrawTextCentered(date2, SCREEN_WIDTH/2, BASE_PARAGRAPH_POS + ADD_PARAGRAPH_POS + BASE_GAP, {0.72, 0.63, 0.81, 1}, font, PARAGRAPH_SCALE)
		if kurwacoto_jest == 3 then
			draw_widget(parsetime(pociagi.connections[3 + additional_offset].departure) .. " - " .. parsetime(pociagi.connections[3 + additional_offset].arrival) .. "   " .. cena3, BASE_PARAGRAPH_POS + (BASE_GAP * 2) - REC_GAP, true)
		else
			draw_widget(parsetime(pociagi.connections[3 + additional_offset].departure) .. " - " .. parsetime(pociagi.connections[3 + additional_offset].arrival) .. "   " .. cena3, BASE_PARAGRAPH_POS + (BASE_GAP * 2) - REC_GAP, false)
		end
		--TextDraw.DrawTextCentered(date3, SCREEN_WIDTH/2, BASE_PARAGRAPH_POS + ADD_PARAGRAPH_POS + (BASE_GAP * 2), {0.72, 0.63, 0.81, 1}, font, PARAGRAPH_SCALE)
	elseif state == "relacje" then

	end
end

function draw_widget(text, y, selected)
	if selected == true then
		DrawRectangle(y, SCREEN_WIDTH, {0.78, 0.78, 0.78, 1}, RECTANGLE_HEIGHT, false, 5)
		TextDraw.DrawText(text, 10, y, {0, 0, 0, 1}, font, PARAGRAPH_SCALE)
	else
		DrawRectangle(y, SCREEN_WIDTH, {1, 1, 1, 1}, RECTANGLE_HEIGHT, false, 5)
		TextDraw.DrawText(text, 10, y, {0, 0, 0, 1}, font, PARAGRAPH_SCALE)
	end
end
function draw_bottom_screen(dt)
	DrawRectangle(0, 400, {0.08,0.04,0.28, 1}, 240, true)
	--TextDraw.DrawTextCentered("Y - Nadchodzące Konwenty", 320/2, 40, {1, 1, 1, 1}, font, 1) 
	TextDraw.DrawTextCentered("Stacja Początkowa: " .. format_station_name(hotstacje[stacjasel]), 320/2, 60, {1, 1, 1, 1}, font, 1.3)
	if state == "article" then
		TextDraw.DrawTextCentered("DPad Góra/Dół - Scrolluj treść", 320/2, 80, {1, 1, 1, 1}, font, 1)
	else 
		TextDraw.DrawTextCentered("Stacja Końcowa: " .. format_station_name(hotstacje[endstacjasel]), 320/2, 80, {1, 1, 1, 1}, font, 1.3)
	end
	--TextDraw.DrawTextCentered("A - Załaduj Treść", 320/2, 100, {1, 1, 1, 1}, font, 1)
	-- if state == "con_info" then
		-- TextDraw.DrawTextCentered("X - Załaduj Cały Opis", 320/2, 120, {1, 1, 1, 1}, font, 1)
	-- end
	-- TextDraw.DrawTextCentered("Start - Zamknij Aplikacje", 320/2, 200, {1, 1, 1, 1}, font, 1)
	-- TextDraw.DrawTextCentered("Select - Aktualności/Relacje", 320/2, 140, {1, 1, 1, 1}, font, 1)
end
local function extract_p_tags(html)
    local paragraphs = {}
	if html:gmatch("<p>(.-)</p>") then
		for p in html:gmatch("<p>(.-)</p>") do
			table.insert(paragraphs, "<p>" .. p .. "</p>")
		end
	elseif html:gmatch("<p (.-)>(.-)</p>") then
		for p in html:gmatch("<p (.-)>(.-)</p>") do
			table.insert(paragraphs, "<p (.-)>" .. p .. "</p>")
		end
	end
    return table.concat(paragraphs, "\n")
end
function capitalize(str)
    return str:sub(1,1):upper() .. str:sub(2):lower()
end

function replace_special_cases(word)
    if word:lower() == "glowny" then
        return "Główny"
	elseif word:lower() == "glowna" then
		return "Główna"
    end
    return word
end

function format_station_name(station)
    local words = {}
    for word in station:gmatch("[^-]+") do
        local capitalized_word = capitalize(word)
        table.insert(words, replace_special_cases(capitalized_word))
    end
    return table.concat(words, " ")
end
function getprice(id)
	local str = id
	refresh_data("https://koleo.pl/api/v2/main/connections/" .. str .. "/price")
	local cena = responded
	
	local output = cena.prices[1].value .. " PLN"
	print(output)
	return output
end
function parsetime(timestring)
	local str = timestring
	
	local output = str:gsub("^.*T(.-)%..*$", "%1")
	
	return output
end
function parsearticle(xml_to_parse)
	local str = xml_to_parse
	
	local output = extract_p_tags(str):gsub("<br/>", ""):gsub("<noscript>", ""):gsub("</noscript>", ""):gsub("<img.-/>", ""):gsub("<p>", ""):gsub("</p>", " "):gsub("<", ""):gsub("strong", ""):gsub("a href=[^>]+", ""):gsub("&amp;", ""):gsub("%(", ""):gsub("%)", ""):gsub("&nbsp;", " "):gsub("/%a", ""):gsub("%a/", ""):gsub("/", ""):gsub(">", "")
	
	return output
end

function parseparagraphs(xml_to_parse)
	local str = xml_to_parse
	
	local output = str:gsub("<p>", ""):gsub("</p>", " ")
	
	return output
end
function parseurl(xml_to_parse)
	local str = xml_to_parse

	-- Use pattern matching to extract the URL path
	local url_path = str:match('href="(.-)"')
	return url_path
end
function update_cena()
	cena1 = getprice(pociagi.connections[selection].id)
	cena2 = getprice(pociagi.connections[selection + 1].id)
	cena3 = getprice(pociagi.connections[selection + 2].id)
end
function dawajpociagi_prosze(startstacja, endstacja)
	refresh_data("https://timeapi.io/api/time/current/zone?timeZone=Europe%2FSarajevo")
	czasenmachen = parsetime(responded.dateTime)
	refresh_data("https://koleo.pl/api/v2/main/connections?query%5Bstart_station%5D=" .. startstacja .. "&query%5Bend_station%5D=" .. endstacja .. "&query%5Bdate%5D=" .. os.date("%Y-%m-%d") .. "T" .. czasenmachen .. "&query%5Bonly_direct%5D=false&query%5Bonly_purchasable%5D=true&query%5Bis_arrival%5D=false")
	pociagi = responded
	update_cena()
	gdzieidokad = format_station_name(hotstacje[stacjasel]) .. " -> " .. format_station_name(hotstacje[endstacjasel])
end
--https://koleo.pl/api/v2/main/connections?query%5Bstart_station%5D=wroclaw-glowny&query%5Bend_station%5D=poznan-glowny&query%5Bdate%5D=2024-10-23T09%3A20%3A30&query%5Bonly_direct%5D=false&query%5Bonly_purchasable%5D=false&query%5Bis_arrival%5D=false

function refresh_data(url)
	print(url)
    -- Headers
    -- local myheaders = {
        -- ["user-agent"] = "Mozilla/5.0 (Windows NT 10.0; rv:129.0) Gecko/20100101 Firefox/129.0",
        -- ["accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/png,image/svg+xml,*/*;q=0.8",
        -- ["sec-fetch-user"] = "?1",
		-- ["sec-fetch-site"] = "none",
        -- ["sec-fetch-mode"] = "navigate",
        -- ["sec-fetch-dest"] = "document",
        -- ["accept-encoding"] = "gzip, deflate, br, zstd",
        -- ["accept-language"] = " pl,en-US;q=0.7,en;q=0.3",
		-- ["upgrade-insecure-requests"] = "1",
		-- ["te"] = "trailers",
		-- ["content-length"] = "0",
        -- ["priority"] = "u=0, i"
    -- }
    -- Response table to collect the response body
    response_body = {}

    -- Making the HTTP request
    code, body, headers = https.request(url, {method = "GET", headers = {["x-koleo-version"] = 1} })
	
    -- Your HTML string to parse
    local html = body

    -- Parse the HTML
	print(body)
    responded = json.decode(body)
	
end
	
function love.draw(screen)
    if screen == "bottom" then
        draw_bottom_screen()
    else
        draw_top_screen()
    end
end 

function love.gamepadpressed(joystick, button)
	if button == "dpright" then
		if mode_sel > 1 then
			if stacjasel < #hotstacje then
				stacjasel = stacjasel + 1
			elseif stacjasel == #hotstacje then
				stajcasel = 1
			end
		else
			if endstacjasel < #hotstacje then
				endstacjasel = endstacjasel + 1
			elseif endstacjasel == #hotstacje then
				endstajcasel = 1
			end
		end
		print(endstacjasel)
	elseif button == "dpleft" then
		if mode_sel > 1 then
			if stacjasel > 1 then
				stacjasel = stacjasel - 1
			elseif stacjasel == 1 then
				stajcasel = #hotstacje
			end
		else
			if endstacjasel > 1 then
				endstacjasel = endstacjasel - 1
			elseif endstacjasel == 1 then
				endstajcasel = #hotstacje
			end
		end
		print(endstacjasel)
	end
	if button == "b" then
		if mode_sel == 1 then 
			refresh_data("https://konwenty-poludniowe.pl")
			REFRESHED = 1
			update_news()
			state = "main_strona"
		else
			refresh_data("https://konwenty-poludniowe.pl/konwenty/relacje-z-konwentow?start=" .. rel_offset)
			REFRESHED = 1
			update_re_re_kurwa_jak_jest_relacja_po_angielsku()
			state = "relacje"
		end
	end
	if button == "y" then
		if REFRESHED == 0 then
			dawajpociagi_prosze(hotstacje[stacjasel], hotstacje[endstacjasel])
			--REFRESHED = 1
			print(parsetime(pociagi.connections[1].arrival))
		end
		--con1 = tree("#upcoming_events li a")[1]:getcontent()
		-- con1 = tree("#upcoming_events li a")[1]:getcontent()
		-- con2 = tree("#upcoming_events li a")[2]:getcontent()
		-- con3 = tree("#upcoming_events li a")[3]:getcontent()
		-- con4 = tree("#upcoming_events li a")[4]:getcontent()
		-- con5 = tree("#upcoming_events li a")[5]:getcontent()
		-- con6 = tree("#upcoming_events li a")[6]:getcontent()
		-- con7 = tree("#upcoming_events li a")[7]:getcontent()
		state = "main_strona"
	end
	if state == "main_strona" or "lista_kon" then
		if button == "dpdown" then
			if state == "main_strona" then
				if selection < 6 then
					selection = selection + 1 
					if kurwacoto_jest > 2 then
						kurwacoto_jest = 1
						additional_offset = additional_offset + 3
						update_cena()
					else
						kurwacoto_jest = kurwacoto_jest + 1
					end
				end
			elseif state == "lista_kon" then
				if conselection < 7 and conurl < 14 then
					conselection = conselection + 1
					conurl = conurl + 2
				end
			elseif state == "relacje" then
				if selection < 9 then
					selection = selection + 1 
					if kurwacoto_jest > 2 then
						kurwacoto_jest = 1
						additional_offset = additional_offset + 3
						update_re_re_kurwa_jak_jest_relacja_po_angielsku()
					else
						kurwacoto_jest = kurwacoto_jest + 1
					end
				end
			end
		elseif button == "dpup" then
		    if state == "main_strona" then
				if selection > 1 then
					selection = selection - 1 
					if kurwacoto_jest < 2 then
						selection = selection - 2
						additional_offset = additional_offset - 3
						update_cena()
						kurwacoto_jest = 1
					else
						kurwacoto_jest = kurwacoto_jest - 1
					end
				end
			elseif state == "lista_kon" then
				if conselection > 1 and conurl > 1 then
					conselection = conselection - 1
					conurl = conurl - 2
				end
			
		    elseif state == "relacje" then
				if selection > 1 then
					selection = selection - 1 
					if kurwacoto_jest < 2 then
						selection = selection - 2
						additional_offset = additional_offset - 3
						update_re_re_kurwa_jak_jest_relacja_po_angielsku()
						kurwacoto_jest = 1
					else
						kurwacoto_jest = kurwacoto_jest - 1
					end
				end
			end
		end
		if button == "a" then
			if state == "main_strona" then	
				gotopage(parseurl(tree(".item-title a")[selection]:gettext()))
				REFRESHED = 1
				loadarticle()
				state = "article"
			elseif state == "lista_kon" then
				gotopage(parseurl(tree("#upcoming_events li")[conurl]:gettext()))
				REFRESHED = 1
				cut_eventdesc = tree(".event_description p")[1]:getcontent()
				event_desc = parsearticle(tree(".event_description")[1]:getcontent())
				state = "con_info"
			elseif state == "relacje" then
				gotopage(parseurl(tree(".page-header h2 a")[selection]:gettext()))
				REFRESHED = 1
				loadarticle()
				state = "article"
			end
		end
		if button == "x" then
			if state == "con_info" then
				state = "con_desc"
			end
		end
		if state == "relacje" then
			if button == "leftshoulder" then
				if additional_offset ~= 0 then
					rel_offset = rel_offset - 9
					refresh_data("https://konwenty-poludniowe.pl/konwenty/relacje-z-konwentow?start=" .. rel_offset)
					update_re_re_kurwa_jak_jest_relacja_po_angielsku()
				end
			elseif button == "rightshoulder" then
				rel_offset = rel_offset + 9
				refresh_data("https://konwenty-poludniowe.pl/konwenty/relacje-z-konwentow?start=" .. rel_offset)
				update_re_re_kurwa_jak_jest_relacja_po_angielsku()
			end
		end
		if button == "start" then
			love.event.quit()
		end
		if button == "back" then 
			if mode_sel > 1 then
				mode_sel = 1
			else
				mode_sel = mode_sel + 1
			end
		end
	end
end


function love.update(dt)
	if state == "article" then
		local joystick = love.joystick.getJoysticks()[1]
		if joystick then 
			if joystick:isGamepadDown("dpup") then
				MOVE_PAGE = MOVE_PAGE + player_speed * dt
			end
			if joystick:isGamepadDown("dpdown") then
				MOVE_PAGE = MOVE_PAGE - player_speed * dt
			end
		end
	end
    love.graphics.origin()  
end

