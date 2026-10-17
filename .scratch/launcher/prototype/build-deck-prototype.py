"""Build the throwaway controller demo as one offline, double-clickable HTML file."""
import base64
from pathlib import Path

root = Path(__file__).parent
html = (root / "deck-controller-source.html").read_text()
for filename, mime in [("spooky-forest.jpg", "image/jpeg"), ("PirataOne-Regular.ttf", "font/ttf")]:
    encoded = base64.b64encode((root / "assets" / filename).read_bytes()).decode("ascii")
    html = html.replace("assets/" + filename, f"data:{mime};base64,{encoded}")
(root / "deck-controller-prototype.html").write_text(html)
print("Built self-contained deck-controller-prototype.html")
