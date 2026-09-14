package pl.danielstrielnikow.cube_backend.model;

import jakarta.persistence.*;
import lombok.Data;

@Entity
@Table(name = "cubes")
@Data
public class Cube {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    private String serialNumber;

    private String name;


}
