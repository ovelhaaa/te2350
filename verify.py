from playwright.sync_api import sync_playwright
import time

def verify_feature():
    with sync_playwright() as p:
        browser = p.chromium.launch(
            args=[
                "--use-fake-ui-for-media-stream",
                "--use-fake-device-for-media-stream",
            ]
        )
        context = browser.new_context(record_video_dir="/home/jules/verification/video")
        page = context.new_page()

        try:
            page.goto("http://localhost:8000")
            page.wait_for_timeout(1000)

            # Start mic to trigger wasm initialization
            page.get_by_text("Start Audio (Mic)").click()
            page.wait_for_timeout(3000) # give it some time to fetch and initialize

            page.screenshot(path="/home/jules/verification/verification.png")
            page.wait_for_timeout(1000)
        finally:
            context.close()
            browser.close()

if __name__ == "__main__":
    import os
    os.makedirs("/home/jules/verification/video", exist_ok=True)
    verify_feature()