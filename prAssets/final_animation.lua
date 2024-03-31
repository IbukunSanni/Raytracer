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


 

-- Example usage
local csv_data = read_csv("prAssets/ball_Control_frame_locations.csv")

if csv_data then
    -- Loop through each key frame
    for _, row in ipairs(csv_data) do 
                io.write(row[1], " KeyFrame \n")  -- print keyframe num at the beginning
                scene = gr.node( 'scene' )
                scene:translate(-3.1, -2.9, -10)

                -- Create background sphere
                bg_sphere = gr.nh_sphere('bg_sphere', {0, 0, 0}, 1)
                bg_sphere:translate(-4, 5.6, -20)
                bg_sphere:set_material(mat2)
                scene:add_child(bg_sphere)

                -- Create foreground box
                fg_box = gr.nh_box('fg_box', {0, 0, 0}, 1)
                fg_box:translate(3.4, 1, 7.8)
                fg_box:set_material(gold)
                scene:add_child(fg_box)

                -- Create plane
                plane = gr.mesh( 'plane', 'Assets/plane.obj' )
                plane:set_material(red)
                plane:scale(4.8, 1, 5.2)
                plane:translate(3, 0, 3.2)
                scene:add_child(plane)

                white_light = gr.light({-3, 2 ,-11}, {0.9, 0.9, 0.9}, {1, 0, 0})
                sun_light = gr.light({150.0, 150.0, -1000.0}, {0.9, 0.9, 0.9}, {1, 0, 0})
                magenta_light = gr.light({400.0, 100.0, -650.0}, {0.7, 0.0, 0.7}, {1, 0, 0})

                -- Create ball Control
                ballControl = gr.node( 'ballControl' )
                scene:add_child(ballControl)

                -- Create ball 
                ball = gr.nh_sphere('ball', {0, 0, 0}, 1)
                ball:translate(0, 1, 0)
                ball:set_material(mat1)
                ballControl:add_child(ball)

                --Apply Transformations
                ballControl:scale(row[5], row[6], row[7])
                ballControl:translate(row[2], row[3], row[4])
                local x = (row[1]-1)*(180/96)
                local xRad = math.rad(x)
                bg_sphere: translate(10 * math.sin(xRad),0,0)
                
                -- Render Frame
                local frameIdx = string.format("%03d",row[1])
                local result = "Renders/bkeytest_frame_" .. frameIdx..".png"
                gr.render(scene, result, 512, 512,
	                        {0, 0, 0}, {0, 0, -1}, {0, 1, 0}, 50,
	                        {0.3, 0.3, 0.3}, {white_light, sun_light})
        io.write("\n")  -- Move to the next line
    end
end
