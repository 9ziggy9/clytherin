FROM gcc:latest

WORKDIR /

COPY . /

RUN make all

EXPOSE 9001

# Set the command to run
CMD ["/build/run"]
