var i = fq_buffer.caretX();
var j = fq_buffer.caretY();
fq_window.writeString("last pos:");
fq_window.writeString(i);
fq_window.writeString("以及");
fq_window.writeString(j);
fq_window.writeString("\n");
var str = fq_buffer.allPlainTextAt(23);
fq_window.writeString(str);
fq_window.writeString("\n");
var str1 = fq_buffer.allAnsiTextAt(23);
fq_window.writeString(str1);
fq_window.writeString("\n");

function myKeyPressEvent(modifiers, key) 
{
if (modifiers & parseInt("0x02000000"))
 {
   fq_window.writeString(key);
 }
}
fq_screen.keyPressEvent.connect(myKeyPressEvent);



function  myMouseEvent(type, x, y, button, buttons, modifiers) 
{
if ((type == 0) && (modifiers & parseInt("0x04000000")) && (button == parseInt("0x00000001"))) 
{
 fq_window.writeString("Char at ");
fq_window.writeString(x);
fq_window.writeString(" ");
fq_window.writeString(y);
fq_window.writeString(" ====>");
 var point = fq_screen.mapPixelToChar(x, y);
fq_window.writeString(point[0]);
fq_window.writeString(" ");
fq_window.writeString(point[1]);
fq_window.writeString(" ");
 var text = fq_buffer.plainTextAt(point[1], point[0], point[0] + 1);
    
 fq_window.writeString(text);
 fq_window.writeString("\n");
}
}
fq_screen.mouseEvent.connect(myMouseEvent);
