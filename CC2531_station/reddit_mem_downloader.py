# Put this script in the same folder as the station.py is
# Get yourself didder from: https://github.com/makeworld-the-better-one/didder
# isntall the needed libs via pip install praw, pillow
# create a folder named meme
# insert your display id
display_id = "0000000000000013"

import requests
from PIL import Image, ImageDraw, ImageFont
import praw
import bmp2grays 
import os
import gzip
import time 

#from here https://stackoverflow.com/questions/4902198/pil-how-to-scale-text-size-in-relation-to-the-size-of-the-image
def find_font_size(text, font, image, target_width_ratio):
    tested_font_size = 100
    tested_font = ImageFont.truetype(font, tested_font_size)
    observed_width = get_text_size(text, image, tested_font)
    estimated_font_size = tested_font_size / (observed_width / image.width) * target_width_ratio
    return round(estimated_font_size)

def get_text_size(text, image, font):
    im = Image.new('RGB', (image.width, image.height))
    draw = ImageDraw.Draw(im)
    return draw.textlength(text, font)

def do_memeing():
    memefolder = "memes/"
    filename = "input_img/" + display_id + ".bmp" 
    downloaded_in = memefolder + "current.jpg"
    pre_dither = memefolder + "raw.png"
    mid_dither = memefolder + "dithered.png"
        
    reddit = praw.Reddit(# create your own application here: https://ssl.reddit.com/prefs/apps/
        client_id="FILL IN YOUR DATA",
        client_secret="FILL IN YOUR DATA",
        user_agent="meme reader",
    )
        
    posts = reddit.subreddit('memes').new(limit=50)
    author = ""
    for post in posts:
        url = (post.url)
        file_name = url.split("/")
        if len(file_name) == 0:
            file_name = re.findall("/(.*?)", url)
        file_name = file_name[-1]
        if ".jpg" in file_name:
            author = 'by: '+ str(post.author)
            with open(downloaded_in,"wb") as f:
                f.write(requests.get(url).content)
            print(author)
            break
            
    image = Image.open(downloaded_in)
    print(image.size)
    
    
    out_img = Image.new("RGB",(384,640), 0)
    image.thumbnail((384,640))
    img_w, img_h = image.size
    bg_w, bg_h = out_img.size
    offset = ((bg_w - img_w) // 2, (bg_h - img_h) // 2)
    out_img.paste(image,offset)
    
    font_size = find_font_size(author, "arial.ttf", image, 0.3)
    font = ImageFont.truetype("arial.ttf", font_size)
    img_draw = ImageDraw.Draw(out_img)
    img_draw.text((0, 0), author, fill='Red', font=font)
    out_img = out_img.rotate(90, expand=True)
    out_img.save(pre_dither)
    out_img.save(filename)
    
    #didder -p "black white red" -i memes/raw.png -o out.png edm -s FloydSteinberg # FROM https://github.com/makeworld-the-better-one/didder
    os.system('didder -p "black white red" -i ' + filename + ' -o ' + mid_dither + ' edm -s FloydSteinberg')
    Image.open(mid_dither).convert("RGB").save(filename)
    
    modification_time = os.path.getmtime(filename)
    creation_time = os.path.getctime(filename)
    imgVer = int(modification_time)<<32|int(creation_time)
    file_conv = os.path.join("tmp/", display_id + "_" + str(imgVer) + ".bmp")
    bmp2grays.convertImage(1, "1bppR", filename, file_conv)
    
    file = open(file_conv,"rb")
    data = file.read()
    file.close()
    compressed_data = gzip.compress(data)
    file = open(file_conv,"wb")
    file.write(compressed_data)
    file.close()
    print("Size before compression: " + str(len(data)) + " compressed: " + str(len(compressed_data)))



while 1:
    print("One round")
    do_memeing()

    starttime = time.time()
    while time.time() < starttime + (60 * 60 * 3):# sleep for 3 hours
        time.sleep(1)