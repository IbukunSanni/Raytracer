-- Function to read a CSV file
function read_csv(filename)
    local file = io.open(filename, "r")  -- Open the file in read mode
    if not file then
        print("Error: Unable to open file:", filename)
        return
    end
    
    local data = {}  -- Table to store CSV data
    -- false to see firstline
    -- true to skip firstline
    local isFirstLine = true -- Flag to skip first line 
    
    -- Read each line in the file
    for line in file:lines() do
        if isFirstLine then
            isFirstLine = false
        else
            local row = {}
            for value in string.gmatch(line, '([^,]+)') do
                table.insert(row, value)  -- Store each value in the row
            end
            table.insert(data, row)  -- Store the row in the data table
        end
    end
    
    file:close()  -- Close the file
    return data
end



mat1 = gr.material({0.7, 1.0, 0.7}, {0.5, 0.7, 0.5}, 25, 0.0, 0.0, 1.0)
mat2 = gr.material({0.5, 0.5, 0.5}, {0.5, 0.7, 0.5}, 25)
mat3 = gr.material({1.0, 0.6, 0.1}, {0.5, 0.7, 0.5}, 25)
mat4 = gr.material({0.7, 0.6, 1.0}, {0.5, 0.4, 0.8}, 25)
blue = gr.material({0.0, 0.25, 0.53}, {0.0, 0.25, 0.53}, 25, 0.4, 0.0, 1.0)
gold = gr.material({0.93, 0.8, 0.38}, {0.91, 0.78, 0.51}, 25)
red = gr.material({0.89, 0.21, 0.22}, {0.89, 0.21, 0.22}, 25)

scene = gr.node( 'scene' )
scene:translate(0, 0, -800)

noon = gr.nh_sphere('noon', {0, 0, 0}, 100)
scene:add_child(noon)
noon:set_material(mat1)


white_light = gr.light({-100.0, 150.0, -400.0}, {0.9, 0.9, 0.9}, {1, 0, 0})
sun_light = gr.light({150.0, 150.0, -1000.0}, {0.9, 0.9, 0.9}, {1, 0, 0})
magenta_light = gr.light({400.0, 100.0, -650.0}, {0.7, 0.0, 0.7}, {1, 0, 0})

-- Example usage
local csv_data = read_csv("prAssets/moon_frame_locations.csv")
if csv_data then
    -- Print the CSV data
    for _, row in ipairs(csv_data) do -- loop through each key frame
        
        -- for idx, value in ipairs(row) do
        --     if idx == 2 then
                io.write(row[1], " KeyFrame \n")  -- Print each value separated by a tab
                
                noon:translate(row[2], row[3], row[4])
                local frameIdx = string.format("%03d",row[1])
                local result = "Renders/test_frame_" .. frameIdx..".png"
                gr.render(scene, result, 512, 512,
	                        {0, 0, 0}, {0, 0, -1}, {0, 1, 0}, 50,
	                        {0.3, 0.3, 0.3}, {white_light, sun_light})
                noon:translate(-row[2], -row[3], -row[4])
        --     end
        -- end
        io.write("\n")  -- Move to the next line
    end
end
