#! /usr/bin/env python3

if __name__ == "__main__":
    from driver.main import main
    from datetime import datetime
    # Get the current date and time
    current_datetime = datetime.now()

    # Format the datetime as per your requirement
    formatted_datetime = current_datetime.strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3]
    print(f"Start time: {formatted_datetime}")
    main()
