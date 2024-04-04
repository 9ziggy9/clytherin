FROM gcc:latest

WORKDIR /

COPY . /

RUN make all

# Set the command to run
CMD ["/build/run"]
